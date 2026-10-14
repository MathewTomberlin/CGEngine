#include <algorithm>
#include <chrono>
#include "VoxelSimulation.h"
#include "VoxelBlocks.h"
#include "VoxelWorld.h"

namespace CGEngine {
	namespace {
		// The six neighbours of a cell, as offsets.
		const int Neighbours[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		// The four neighbours beside a cell, in the horizontal plane.
		const int Sides[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		// Cells in a key: x and z are offset by 2^20 so they are never negative, y is below 2^21.
		const int64_t KeyBias = int64_t(1) << 20;
		const int64_t KeyMask21 = (int64_t(1) << 21) - 1;
	}

	int64_t VoxelSimulation::keyOf(int x, int y, int z) {
		return ((static_cast<int64_t>(x) + KeyBias) << 42) | ((static_cast<int64_t>(z) + KeyBias) << 21) | static_cast<int64_t>(y);
	}

	void VoxelSimulation::unkey(int64_t key, int& x, int& y, int& z) {
		y = static_cast<int>(key & KeyMask21);
		z = static_cast<int>(((key >> 21) & KeyMask21) - KeyBias);
		x = static_cast<int>((key >> 42) - KeyBias);
	}

	float VoxelSimulation::nextRandom() {
		// xorshift32: repeatable from the same start, so a world's simulation is the same every run.
		randomState ^= randomState << 13;
		randomState ^= randomState >> 17;
		randomState ^= randomState << 5;
		return static_cast<float>(randomState & 0xFFFFFFu) / static_cast<float>(0x1000000u);
	}

	void VoxelSimulation::activate(int x, int y, int z) {
		if (y < 0) return;
		const int64_t key = keyOf(x, y, z);
		if (queued.insert(key).second) queue.push_back(key);
	}

	void VoxelSimulation::set(VoxelWorld& world, int x, int y, int z, uint8_t block) {
		// VoxelWorld::setBlock queues the changed cell and its neighbours, so the change is seen on the next tick.
		world.setBlock(x, y, z, block);
	}

	void VoxelSimulation::beginMove(VoxelWorld& world, int fromX, int fromY, int fromZ, int toX, int toY, int toZ, uint8_t block) {
		Move move;
		move.from[0] = fromX;
		move.from[1] = fromY;
		move.from[2] = fromZ;
		move.to[0] = toX;
		move.to[1] = toY;
		move.to[2] = toZ;
		move.block = block;
		move.elapsed = 0.0;
		move.duration = moveDuration;
		reserved.insert(keyOf(toX, toY, toZ));
		moves.push_back(move);
		// The source is cleared now, so the block is drawn leaving it rather than in two places.
		set(world, fromX, fromY, fromZ, 0);
	}

	void VoxelSimulation::advanceMoves(VoxelWorld& world, double dt) {
		for (size_t i = 0; i < moves.size();) {
			moves[i].elapsed += dt;
			if (moves[i].elapsed < moves[i].duration) {
				++i;
				continue;
			}
			// The move ends: the block lands at its destination, and the cell is free for others again.
			const Move landed = moves[i];
			moves[i] = moves.back();
			moves.pop_back();
			reserved.erase(keyOf(landed.to[0], landed.to[1], landed.to[2]));
			set(world, landed.to[0], landed.to[1], landed.to[2], landed.block);
		}
	}

	void VoxelSimulation::settleMoves(VoxelWorld& world) {
		while (!moves.empty()) {
			const Move landed = moves.back();
			moves.pop_back();
			reserved.erase(keyOf(landed.to[0], landed.to[1], landed.to[2]));
			set(world, landed.to[0], landed.to[1], landed.to[2], landed.block);
		}
	}

	void VoxelSimulation::tickCell(VoxelWorld& world, int x, int y, int z) {
		const uint8_t block = static_cast<uint8_t>(world.getBlock(x, y, z));
		if (block == 0 || !world.loadedAt(x, z)) return;

		if (hook) {
			const int replacement = hook(x, y, z, block);
			if (replacement >= 0 && replacement != block) {
				set(world, x, y, z, static_cast<uint8_t>(replacement));
				return;
			}
		}

		for (const Rule& rule : rules) {
			if (rule.block != block) continue;
			bool touching = false;
			for (const auto& d : Neighbours) {
				if (world.getBlock(x + d[0], y + d[1], z + d[2]) == rule.neighbour) {
					touching = true;
					break;
				}
			}
			if (touching && nextRandom() < rule.chance) {
				set(world, x, y, z, rule.becomes);
				return;
			}
		}

		const BlockType& type = world.blockType(block);
		if (type.falls && y > 0) {
			// Falling: into air (a move in flight if the game animates it, else at once), or swapping down past a liquid.
			const uint8_t below = static_cast<uint8_t>(world.getBlock(x, y - 1, z));
			if (below == 0 && !reserved.count(keyOf(x, y - 1, z))) {
				if (moveDuration > 0.0) {
					beginMove(world, x, y, z, x, y - 1, z, block);
				} else {
					set(world, x, y - 1, z, block);
					set(world, x, y, z, 0);
				}
				return;
			}
			if (below != 0 && world.blockType(below).liquid) {
				set(world, x, y - 1, z, block);
				set(world, x, y, z, below);
				return;
			}
		}

		if (type.liquid && type.flows) {
			// Water falls into air below, and otherwise spreads sideways one level at a time, so a flow is finite.
			const int level = Water::level(block);
			if (y > 0 && world.getBlock(x, y - 1, z) == 0) {
				set(world, x, y - 1, z, Water::flowing(7));
			} else if (level > 1) {
				for (const auto& side : Sides) {
					const int nx = x + side[0];
					const int nz = z + side[1];
					if (world.getBlock(nx, y, nz) == 0) set(world, nx, y, nz, Water::flowing(level - 1));
				}
			}
			// A flow that nothing feeds drains: water above feeds level 7, a neighbour one level more feeds one less.
			if (block != Water::Source) {
				int feed = Water::level(static_cast<uint8_t>(world.getBlock(x, y + 1, z))) > 0 ? 7 : 0;
				for (const auto& side : Sides) {
					const int neighbour = Water::level(static_cast<uint8_t>(world.getBlock(x + side[0], y, z + side[1])));
					feed = std::max(feed, neighbour - 1);
				}
				if (feed < level) set(world, x, y, z, 0);
			}
		}
	}

	int VoxelSimulation::simulate(VoxelWorld& world, double dt, double budgetMs, double tickSeconds) {
		advanceMoves(world, dt);
		accumulator += dt;
		int looked = 0;
		const auto start = std::chrono::steady_clock::now();
		bool outOfTime = false;
		while (accumulator >= tickSeconds && !outOfTime) {
			accumulator -= tickSeconds;
			// The cells queued so far are this tick's. Changes made during the tick queue the next one.
			std::vector<int64_t> batch;
			batch.swap(queue);
			queued.clear();
			for (size_t n = 0; n < batch.size(); ++n) {
				const double spent = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
				if (spent >= budgetMs) {
					// Out of time: the rest wait for the next call.
					for (size_t rest = n; rest < batch.size(); ++rest) {
						int x, y, z;
						unkey(batch[rest], x, y, z);
						activate(x, y, z);
					}
					outOfTime = true;
					break;
				}
				int x, y, z;
				unkey(batch[n], x, y, z);
				tickCell(world, x, y, z);
				++looked;
			}
		}
		// A long pause does not turn into a burst of ticks.
		accumulator = std::min(accumulator, tickSeconds);
		return looked;
	}
}
