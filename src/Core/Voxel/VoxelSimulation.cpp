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
		// How far a volume's unit looks along a level for a drop (see downhillSide), in cells.
		const int DownhillReach = 4;
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

	namespace {
		// Whether a neighbour of the cell holds one of the kinds: one of the six faces, or with diagonal, any of the 26 around it.
		bool touching(VoxelWorld& world, int x, int y, int z, const std::bitset<256>& kinds, bool diagonal) {
			for (int dx = -1; dx <= 1; ++dx) {
				for (int dy = -1; dy <= 1; ++dy) {
					for (int dz = -1; dz <= 1; ++dz) {
						const int faces = (dx != 0) + (dy != 0) + (dz != 0);
						if (faces == 0 || (!diagonal && faces > 1)) continue;
						if (kinds[static_cast<uint8_t>(world.getBlock(x + dx, y + dy, z + dz))]) return true;
					}
				}
			}
			return false;
		}
	}

	// The reactions of a cell: each rule of the block the cell holds, whose neighbours hold one of its kinds. A rule without time
	// changes the block with its chance per tick. A timed rule shows its progress as damage stages (out of the block's hits, as a hit
	// shows) and changes the block when the time is up. Air is a block too, so a rule on air grows a block into it.
	bool VoxelSimulation::react(VoxelWorld& world, int x, int y, int z, uint8_t block, double tickSeconds) {
		const int64_t key = keyOf(x, y, z);
		bool reacting = false;
		for (size_t r = 0; r < rules.size() && !reacting; ++r) {
			const Rule& rule = rules[r];
			if (rule.block != block || !touching(world, x, y, z, rule.neighbours, rule.diagonal)) continue;
			if (rule.seconds <= 0.0f) {
				if (nextRandom() < rule.chance) {
					set(world, x, y, z, rule.becomes);
					return true;
				}
				continue;
			}
			const int hits = std::max(1, static_cast<int>(world.blockType(block).hits));
			auto it = progress.find(key);
			if (it == progress.end() || it->second.rule != r) {
				// A reaction starts here. A cell already damaged (a reaction saved with the edits) carries on from its stage.
				const double stage = world.damageAt(x, y, z);
				it = progress.insert_or_assign(key, Progress{ static_cast<uint32_t>(r), rule.seconds * stage / hits }).first;
			}
			it->second.elapsed += tickSeconds;
			if (it->second.elapsed >= rule.seconds) {
				progress.erase(it);
				set(world, x, y, z, rule.becomes);   // the block breaks into the new one (setBlock clears its damage)
				return true;
			}
			const int stage = std::min(hits - 1, static_cast<int>(it->second.elapsed * hits / rule.seconds));
			if (world.damageAt(x, y, z) != stage) world.setDamage(x, y, z, stage);
			reacting = true;
		}
		if (reacting) {
			activate(x, y, z);   // looked at again on the next tick, while the reaction runs
		} else {
			// Nothing reacts now: a reaction that was under way stops, and the damage it showed heals.
			auto it = progress.find(key);
			if (it != progress.end()) {
				progress.erase(it);
				world.setDamage(x, y, z, 0);
			}
		}
		return false;
	}

	void VoxelSimulation::tickCell(VoxelWorld& world, int x, int y, int z, double tickSeconds) {
		const uint8_t block = static_cast<uint8_t>(world.getBlock(x, y, z));
		if (!world.loadedAt(x, z)) return;
		if (block == 0) {
			react(world, x, y, z, block, tickSeconds);
			return;
		}

		if (hook) {
			const int replacement = hook(x, y, z, block);
			if (replacement >= 0 && replacement != block) {
				set(world, x, y, z, static_cast<uint8_t>(replacement));
				return;
			}
		}

		if (react(world, x, y, z, block, tickSeconds)) return;

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

		if (const VolumeLiquid* volume = volumeOf(block)) {
			tickVolume(world, x, y, z, *volume);
			return;
		}

		if (type.liquid && type.flows) {
			// A liquid (water or lava) falls into air below, and otherwise spreads sideways one level at a time, so a flow is finite.
			const Liquid* liquid = liquidOf(block);
			const int level = liquid ? liquid->level(block) : 0;
			if (liquid && y > 0 && world.getBlock(x, y - 1, z) == 0) {
				set(world, x, y - 1, z, liquid->flowing(7));
			} else if (liquid && level > 1) {
				for (const auto& side : Sides) {
					const int nx = x + side[0];
					const int nz = z + side[1];
					if (world.getBlock(nx, y, nz) == 0) set(world, nx, y, nz, liquid->flowing(level - 1));
				}
			}
			// A flow that nothing feeds drains: the same liquid above feeds level 7, a neighbour one level more feeds one less.
			if (liquid && block != liquid->source) {
				int feed = liquid->level(static_cast<uint8_t>(world.getBlock(x, y + 1, z))) > 0 ? 7 : 0;
				for (const auto& side : Sides) {
					const int neighbour = liquid->level(static_cast<uint8_t>(world.getBlock(x + side[0], y, z + side[1])));
					feed = std::max(feed, neighbour - 1);
				}
				if (feed < level) set(world, x, y, z, 0);
			}
		}
	}

	void VoxelSimulation::tickVolume(VoxelWorld& world, int x, int y, int z, const VolumeLiquid& volume) {
		// A volume's units, and the units of the spring's flows of the same liquid (a flow holds its level). Other blocks hold none.
		const auto unitsOf = [&volume](uint8_t block) {
			if (const int units = volume.units(block)) return units;
			const Liquid* flow = liquidOf(block);
			return (flow && flow->source == volume.spring) ? flow->level(block) : 0;
		};
		const int units = unitsOf(static_cast<uint8_t>(world.getBlock(x, y, z)));
		if (units == 0) return;
		if (y > 0) {
			// Falling: the whole volume drops into air, or fills the liquid below up to a full cell.
			const uint8_t below = static_cast<uint8_t>(world.getBlock(x, y - 1, z));
			if (below == 0 && !reserved.count(keyOf(x, y - 1, z))) {
				set(world, x, y - 1, z, volume.block(units));
				set(world, x, y, z, 0);
				return;
			}
			const int belowUnits = unitsOf(below);
			if (belowUnits > 0 && belowUnits < VolumeLiquid::Full) {
				const int moved = std::min(units, VolumeLiquid::Full - belowUnits);
				set(world, x, y - 1, z, volume.block(belowUnits + moved));
				set(world, x, y, z, moved == units ? 0 : volume.block(units - moved));
				return;
			}
		}
		// Evening out: a unit moves to a side that is air, or the same liquid with at least two units less. Air with open air
		// below it is a drop: a unit moves over it at once, and falls there, so a volume runs off an edge and downhill.
		for (const auto& side : Sides) {
			const int nx = x + side[0];
			const int nz = z + side[1];
			if (!world.loadedAt(nx, nz)) continue;
			const uint8_t neighbour = static_cast<uint8_t>(world.getBlock(nx, y, nz));
			const int sideUnits = unitsOf(neighbour);
			if (neighbour != 0 && sideUnits == 0) continue;   // another kind of block holds its own liquid
			const int mine = unitsOf(static_cast<uint8_t>(world.getBlock(x, y, z)));
			if (mine == 0) return;
			const bool drop = neighbour == 0 && y > 0 && world.getBlock(nx, y - 1, nz) == 0;
			if (!drop && mine - sideUnits < 2) continue;
			set(world, nx, y, nz, volume.block(sideUnits + 1));
			// The last unit leaves the cell as air: volume.block(0) is the id below the family (127 for water).
			set(world, x, y, z, mine - 1 == 0 ? 0 : volume.block(mine - 1));
		}
		// Downhill: a unit with no drop beside it still flows to a drop that is near, over level air (a flat step on a slope), so a
		// trickle keeps going down a slope instead of stopping at its first step. A pool spreads by the pressure above, not by this.
		const int mineNow = unitsOf(static_cast<uint8_t>(world.getBlock(x, y, z)));
		if (mineNow > 0 && y > 0) {
			const int side = downhillSide(world, x, y, z);
			if (side >= 0) {
				const int nx = x + Sides[side][0];
				const int nz = z + Sides[side][1];
				set(world, nx, y, nz, volume.block(1));
				set(world, x, y, z, mineNow - 1 == 0 ? 0 : volume.block(mineNow - 1));
			}
		}
	}

	int VoxelSimulation::downhillSide(VoxelWorld& world, int x, int y, int z) {
		// Breadth-first over the air at this level, DownhillReach steps at most. Returns the side (index into Sides) of the first step
		// toward the nearest cell with air beneath it, or -1 when no drop is that near.
		struct Step {
			int x, z, first, depth;
		};
		auto air = [&](int cx, int cz) { return world.loadedAt(cx, cz) && world.getBlock(cx, y, cz) == 0; };
		std::vector<Step> frontier;
		std::unordered_set<int64_t> seen;
		seen.insert(keyOf(x, y, z));
		for (int i = 0; i < 4; ++i) {
			const int nx = x + Sides[i][0];
			const int nz = z + Sides[i][1];
			if (!air(nx, nz) || !seen.insert(keyOf(nx, y, nz)).second) continue;
			frontier.push_back({ nx, nz, i, 1 });
		}
		for (size_t head = 0; head < frontier.size(); ++head) {
			const Step step = frontier[head];
			if (y > 0 && world.getBlock(step.x, y - 1, step.z) == 0) return step.first;
			if (step.depth >= DownhillReach) continue;
			for (int i = 0; i < 4; ++i) {
				const int nx = step.x + Sides[i][0];
				const int nz = step.z + Sides[i][1];
				if (!air(nx, nz) || !seen.insert(keyOf(nx, y, nz)).second) continue;
				frontier.push_back({ nx, nz, step.first, step.depth + 1 });
			}
		}
		return -1;
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
				tickCell(world, x, y, z, tickSeconds);
				++looked;
			}
		}
		// A long pause does not turn into a burst of ticks.
		accumulator = std::min(accumulator, tickSeconds);
		return looked;
	}
}
