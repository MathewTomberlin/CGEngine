#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include "VoxelTerrain.h"

namespace CGEngine {
	namespace {
		// A repeatable number in [0, 1] for a lattice point. Same idea as the Python sample's hash.
		float latticeValue(int32_t ix, int32_t iz, uint32_t seed) {
			uint32_t h = static_cast<uint32_t>(ix) * 374761393u + static_cast<uint32_t>(iz) * 668265263u + seed * 1442695041u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return static_cast<float>((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
		}

		// A repeatable number in [0, 1] for a block's position: every chunk that sees the block agrees on it.
		float blockValue(int32_t wx, int32_t wy, int32_t wz, uint32_t seed) {
			uint32_t h = static_cast<uint32_t>(wx) * 374761393u + static_cast<uint32_t>(wy) * 2246822519u
				+ static_cast<uint32_t>(wz) * 668265263u + seed * 1442695041u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return static_cast<float>((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
		}

		// Ores: iron is rare, and only in the stone under the ground (see IronCover); coal is more common, at any depth.
		const float IronChance = 0.006f;
		const float CoalChance = 0.02f;
		// Iron lies at least this many blocks under its column's ground, so a dig from the surface, or a cave wall, shows it.
		const int IronCover = 3;
		// Veins: iron also runs through the stone beside a cave, where a player walking the cave sees it. A stone block is a candidate when
		// its hash is under VeinCandidate (checked first, it is cheap); the vein is where a noise field is over VeinLevel, and the block
		// has a cave beside it. The noise is stretched across the blocks so a vein runs for a few blocks, not a single cell.
		const float VeinCandidate = 0.05f;
		const float VeinLevel = 0.62f;
		// Caves at or below this height are full of lava instead of air, so the deep caves hold lakes of it.
		const int LavaLevel = 6;
		// The sea: the sea field (VoxelTerrain::seaWeight) is full sea below SeaDeep and dry land above SeaShore.
		const float SeaDeep = 0.26f;
		const float SeaShore = 0.36f;
		const float SpawnDry = 24.0f;
		const float SpawnFade = 24.0f;

		// Smooth noise in [0, 1]: lattice values blended with smoothstep.
		float valueNoise(float x, float z, uint32_t seed) {
			const float fx = std::floor(x);
			const float fz = std::floor(z);
			const int32_t x0 = static_cast<int32_t>(fx);
			const int32_t z0 = static_cast<int32_t>(fz);
			const float tx = (x - fx) * (x - fx) * (3.0f - 2.0f * (x - fx));
			const float tz = (z - fz) * (z - fz) * (3.0f - 2.0f * (z - fz));
			const float a = latticeValue(x0, z0, seed);
			const float b = latticeValue(x0 + 1, z0, seed);
			const float c = latticeValue(x0, z0 + 1, seed);
			const float d = latticeValue(x0 + 1, z0 + 1, seed);
			const float top = a + (b - a) * tx;
			const float bottom = c + (d - c) * tx;
			return top + (bottom - top) * tz;
		}

		// Three-dimensional value noise in [0, 1], trilinear between lattice points with smoothstep weights.
		float latticeValue3(int32_t ix, int32_t iy, int32_t iz, uint32_t seed) {
			uint32_t h = static_cast<uint32_t>(ix) * 374761393u + static_cast<uint32_t>(iy) * 2246822519u
				+ static_cast<uint32_t>(iz) * 668265263u + seed * 1442695041u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return static_cast<float>((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
		}

		float valueNoise3(float x, float y, float z, uint32_t seed) {
			const float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
			const int32_t x0 = static_cast<int32_t>(fx), y0 = static_cast<int32_t>(fy), z0 = static_cast<int32_t>(fz);
			const float tx = (x - fx) * (x - fx) * (3.0f - 2.0f * (x - fx));
			const float ty = (y - fy) * (y - fy) * (3.0f - 2.0f * (y - fy));
			const float tz = (z - fz) * (z - fz) * (3.0f - 2.0f * (z - fz));
			auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
			const float c00 = lerp(latticeValue3(x0, y0, z0, seed), latticeValue3(x0 + 1, y0, z0, seed), tx);
			const float c10 = lerp(latticeValue3(x0, y0 + 1, z0, seed), latticeValue3(x0 + 1, y0 + 1, z0, seed), tx);
			const float c01 = lerp(latticeValue3(x0, y0, z0 + 1, seed), latticeValue3(x0 + 1, y0, z0 + 1, seed), tx);
			const float c11 = lerp(latticeValue3(x0, y0 + 1, z0 + 1, seed), latticeValue3(x0 + 1, y0 + 1, z0 + 1, seed), tx);
			return lerp(lerp(c00, c10, ty), lerp(c01, c11, ty), tz);
		}

		// Four octaves of value noise, each at half the scale and amplitude of the last. In [0, 1].
		float fractalNoise(float x, float z, uint32_t seed) {
			float total = 0.0f;
			float norm = 0.0f;
			float amplitude = 1.0f;
			float frequency = 1.0f / 32.0f;
			for (int octave = 0; octave < 4; ++octave) {
				total += amplitude * valueNoise(x * frequency, z * frequency, seed + static_cast<uint32_t>(octave) * 101u);
				norm += amplitude;
				amplitude *= 0.5f;
				frequency *= 2.0f;
			}
			return total / norm;
		}
	}

	float VoxelTerrain::seaWeight(int x, int z) const {
		// A slow field over about 120 blocks: where it is low, the land sinks into a basin. Full sea below SeaDeep, land above SeaShore.
		const float n = valueNoise(static_cast<float>(x) / 120.0f, static_cast<float>(z) / 120.0f, seed + 7919u);
		const float t = std::clamp((SeaShore - n) / (SeaShore - SeaDeep), 0.0f, 1.0f);
		// No sea near the spawn (the origin): the land fades back in between SpawnDry and SpawnDry + SpawnFade blocks out.
		const float distance = std::sqrt(static_cast<float>(x) * x + static_cast<float>(z) * z);
		const float dry = std::clamp((distance - SpawnDry) / SpawnFade, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t) * dry * dry * (3.0f - 2.0f * dry);
	}

	int VoxelTerrain::columnHeight(int x, int z) const {
		const float n = fractalNoise(static_cast<float>(x), static_cast<float>(z), seed);
		const int land = baseHeight + static_cast<int>(std::pow(n, 1.6f) * static_cast<float>(heightRange));
		const float sea = seaWeight(x, z);
		if (sea <= 0.0f) return land;
		// The sea floor lies 4 to 7 blocks under the sea level, so the water over it is that deep; it wanders a little.
		const float floorHeight = static_cast<float>(seaLevel) - 4.0f - 3.0f * valueNoise(static_cast<float>(x) / 10.0f, static_cast<float>(z) / 10.0f, seed + 9001u);
		return static_cast<int>(std::lround(static_cast<float>(land) + (floorHeight - static_cast<float>(land)) * sea));
	}

	VoxelTerrain::Biome VoxelTerrain::biomeAt(int x, int z) const {
		if (seaWeight(x, z) > 0.5f) return Biome::Sea;
		// One noise field over about 90 blocks: the regions are large, so a desert or a forest is a place to walk through.
		const float n = valueNoise(static_cast<float>(x) / 90.0f, static_cast<float>(z) / 90.0f, seed + 4241u);
		if (n > 0.7f) return Biome::Desert;
		if (n < 0.3f) return Biome::Forest;
		return Biome::Plains;
	}

	void VoxelTerrain::fillChunk(int cx, int cz, int size, int height, std::vector<uint8_t>& blocks) const {
		// The column, one section at a time: the same blocks its sections hold (see fillSection).
		const size_t n = static_cast<size_t>(size) * size * size;
		blocks.assign(n * static_cast<size_t>(height / size), Air);
		for (int cy = 0; cy < height / size; ++cy) {
			std::vector<uint8_t> section;
			fillSection(cx, cy, cz, size, section);
			std::copy(section.begin(), section.end(), blocks.begin() + static_cast<std::ptrdiff_t>(cy * n));
		}
	}

	VoxelTerrain::ColumnMemo VoxelTerrain::memoColumn(int wx, int wz) const {
		thread_local std::unordered_map<uint64_t, ColumnMemo> memo;
		const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(wx)) << 32) | static_cast<uint32_t>(wz);
		auto found = memo.find(key);
		if (found != memo.end()) {
			const ColumnMemo& c = found->second;
			if (c.seed == seed && c.seaLevel == seaLevel && c.baseHeight == baseHeight && c.heightRange == heightRange && c.snowLevel == snowLevel) {
				return c;
			}
		}
		const ColumnMemo fresh{ seed, seaLevel, baseHeight, heightRange, snowLevel, columnHeight(wx, wz), static_cast<uint8_t>(biomeAt(wx, wz)) };
		// A worker's memo is cleared when it grows large: a column is worked out again when it is next needed.
		if (memo.size() >= (1u << 18)) memo.clear();
		memo[key] = fresh;
		return fresh;
	}

	VoxelTerrain::Column VoxelTerrain::columnAt(int wx, int wz) const {
		Column column;
		const ColumnMemo memo = memoColumn(wx, wz);
		column.top = memo.top;
		if (column.top <= seaLevel + 1) {
			column.surface = Sand;
		} else if (column.top >= snowLevel) {
			column.surface = Snow;
		} else if (static_cast<Biome>(memo.biome) == Biome::Desert) {
			column.surface = Sand;
		} else {
			column.surface = Grass;
		}
		return column;
	}

	bool VoxelTerrain::besideCave(int wx, int wy, int wz) const {
		// Whether a cave (or the lava in a deep one) is in one of the six blocks beside (wx, wy, wz).
		const int dirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for (const auto& d : dirs) {
			if (isCave(wx + d[0], wy + d[1], wz + d[2])) return true;
		}
		return false;
	}

	float VoxelTerrain::cloudCover(int wx, int wz, double t) const {
		// Clouds drift at about two blocks a second: the field moves across the world with the time.
		const float drift = static_cast<float>(t / 24.0);
		return valueNoise(static_cast<float>(wx) / 40.0f + drift, static_cast<float>(wz) / 40.0f, seed + 7001u);
	}

	bool VoxelTerrain::islandAt(int wx, int wy, int wz) const {
		if (wy < IslandMin || wy > IslandMax) return false;
		// Where the islands are: a field over about 90 blocks, so an island region is a few hundred blocks across.
		if (valueNoise(static_cast<float>(wx) / 90.0f, static_cast<float>(wz) / 90.0f, seed + 12011u) < 0.66f) return false;
		// The island's shape: 3D noise over a few blocks, so it is a blob, thinner at its edges.
		return valueNoise3(wx / 9.0f, wy / 6.0f, wz / 9.0f, seed + 12049u) > 0.6f;
	}

	uint8_t VoxelTerrain::cellBlock(int wx, int y, int wz, const Column& column) const {
		if (islandAt(wx, y, wz)) {
			// The island's top is grass, the blocks under it are dirt, and the core is stone.
			if (!islandAt(wx, y + 1, wz)) return Grass;
			if (!islandAt(wx, y + 3, wz)) return Dirt;
			return Stone;
		}
		// Runs bottom to top: stone, dirt, the surface block, and water above a low column. The floor is not special: the stone goes on
		// below y = 0, and so do the caves and the ores.
		const int top = column.top;
		uint8_t block = Air;
		if (y < top - 4) {
			block = Stone;
		} else if (y < top - 1) {
			block = Dirt;
		} else if (y < top) {
			block = column.surface;
		} else if (top < seaLevel && y < seaLevel) {
			block = EndlessWater;
		}
		// Caves are carved below the surface, which stays whole, so they open out only where the noise reaches it.
		if (caveWidth > 0.0f && y < top - 2 && isCave(wx, y, wz)) {
			block = y <= LavaLevel ? EndlessLava : Air;
		}
		// Ores: a stone block becomes iron or coal by its position's value, so the same block is the same in any section.
		if (block == Stone) {
			const float r = blockValue(wx, y, wz, seed);
			if (r < IronChance && y < top - IronCover) {
				block = Iron;
			} else if (caveWidth > 0.0f && r < VeinCandidate && y < top - IronCover
				&& valueNoise(wx * 0.3f + y * 0.12f, wz * 0.3f - y * 0.12f, seed + 6151u) > VeinLevel && besideCave(wx, y, wz)) {
				block = Iron;
			} else if (r < IronChance + CoalChance) {
				block = Coal;
			}
		}
		return block;
	}

	void VoxelTerrain::fillSection(int cx, int cy, int cz, int size, std::vector<uint8_t>& blocks, bool features) const {
		// A section is size cubed: block (x, y, z) at (ly * size + lz) * size + lx, where y is the block's height above the section's floor.
		blocks.assign(static_cast<size_t>(size) * size * size, Air);
		const int y0 = cy * size;
		for (int lz = 0; lz < size; ++lz) {
			for (int lx = 0; lx < size; ++lx) {
				const int wx = cx * size + lx;
				const int wz = cz * size + lz;
				// A column's height and surface do not depend on the section, so each section works them out for its own cells.
				const Column column = columnAt(wx, wz);
				for (int ly = 0; ly < size; ++ly) {
					blocks[(static_cast<size_t>(ly) * size + lz) * size + lx] = cellBlock(wx, y0 + ly, wz, column);
				}
			}
		}
		if (features) {
			placeTrees(cx, cy, cz, size, blocks);
			placeRocks(cx, cy, cz, size, blocks);
		}
	}

	bool VoxelTerrain::isCave(int wx, int wy, int wz) const {
		// Tunnels: the band where two noise fields are both near 0.5 is a network of winding passages. Rooms: rare spots
		// where a coarser field is high.
		const float x = wx / 22.0f, y = wy / 16.0f, z = wz / 22.0f;
		// The second field is read only where the first is in the band (most cells are not), so most cells read one field here.
		const float a = valueNoise3(x, y, z, seed + 5003u) - 0.5f;
		if (std::fabs(a) < caveWidth) {
			const float b = valueNoise3(x + 17.3f, y + 9.1f, z + 4.7f, seed + 7019u) - 0.5f;
			if (std::fabs(b) < caveWidth) return true;
		}
		return valueNoise3(wx / 40.0f, wy / 30.0f, wz / 40.0f, seed + 9011u) > 0.80f;
	}

	void VoxelTerrain::placeRocks(int cx, int cy, int cz, int size, std::vector<uint8_t>& blocks) const {
		const int margin = 3;   // a boulder is at most two blocks out, so the ones rooted up to 3 blocks outside reach in
		// A cell in this section (the boulder's cells that lie in other sections are those sections' to place).
		auto place = [&](int wx, int y, int wz, uint8_t block) {
			const int lx = wx - cx * size;
			const int ly = y - cy * size;
			const int lz = wz - cz * size;
			if (lx < 0 || lx >= size || lz < 0 || lz >= size || ly < 0 || ly >= size) return;
			uint8_t& cell = blocks[(static_cast<size_t>(ly) * size + lz) * size + lx];
			if (cell == Air) cell = block;
		};
		for (int wz = cz * size - margin; wz < (cz + 1) * size + margin; ++wz) {
			for (int wx = cx * size - margin; wx < (cx + 1) * size + margin; ++wx) {
				const ColumnMemo memo = memoColumn(wx, wz);
				const Biome biome = static_cast<Biome>(memo.biome);
				const float chance = biome == Biome::Desert ? rockChance * 2.0f : (biome == Biome::Forest ? rockChance * 0.5f : rockChance);
				if (latticeValue(wx, wz, seed + 9013u) >= chance) continue;
				const int ground = memo.top;
				if (ground <= seaLevel + 1 || ground >= snowLevel - 2) continue;
				const int radius = latticeValue(wx, wz, seed + 9019u) < 0.4f ? 2 : 1;
				// The bottom layer sits on the ground: the boulder is a ball of radius r whose lowest layer is the first air above.
				for (int dy = 0; dy <= 2 * radius; ++dy) {
					for (int dz = -radius; dz <= radius; ++dz) {
						for (int dx = -radius; dx <= radius; ++dx) {
							if (dx * dx + (dy - radius) * (dy - radius) + dz * dz > radius * radius + radius / 2) continue;
							place(wx + dx, ground + dy, wz + dz, Stone);
						}
					}
				}
			}
		}
	}

		// A tree stands on a grass column, and whether it does is a hash of the column, so every section that sees the tree draws
		// the same one. A trunk of 4 to 6 blocks ends in a crown: two layers of radius 2 (corners cut), one of radius 1 around
		// the trunk top, and a tip.
	void VoxelTerrain::placeTrees(int cx, int cy, int cz, int size, std::vector<uint8_t>& blocks) const {
		// The crown reaches 2 blocks from the trunk, so the trees rooted up to 3 blocks outside the section's columns can reach into it.
		const int margin = 3;
		auto place = [&](int wx, int y, int wz, uint8_t block) {
			const int lx = wx - cx * size;
			const int ly = y - cy * size;
			const int lz = wz - cz * size;
			if (lx < 0 || lx >= size || lz < 0 || lz >= size || ly < 0 || ly >= size) return;
			uint8_t& cell = blocks[(static_cast<size_t>(ly) * size + lz) * size + lx];
			if (cell == Air) cell = block;
		};
		for (int wz = cz * size - margin; wz < (cz + 1) * size + margin; ++wz) {
			for (int wx = cx * size - margin; wx < (cx + 1) * size + margin; ++wx) {
				// Forests grow about three times the trees of the plains, and deserts none.
				const ColumnMemo memo = memoColumn(wx, wz);
				const Biome biome = static_cast<Biome>(memo.biome);
				if (biome == Biome::Desert) continue;
				const float chance = biome == Biome::Forest ? treeChance * 3.0f : treeChance;
				if (latticeValue(wx, wz, seed + 9001u) >= chance) continue;
				const int ground = memo.top;
				if (ground <= seaLevel + 1 || ground >= snowLevel - 2) continue;
				const int trunk = 4 + static_cast<int>(latticeValue(wx, wz, seed + 9007u) * 2.999f);
				const int topY = ground + trunk - 1;
				for (int y = ground; y <= topY; ++y) place(wx, y, wz, Wood);
				for (int dy = -2; dy <= 1; ++dy) {
					const int radius = dy <= -1 ? 2 : (dy == 0 ? 1 : 0);
					for (int dz = -radius; dz <= radius; ++dz) {
						for (int dx = -radius; dx <= radius; ++dx) {
							if (radius == 2 && std::abs(dx) == 2 && std::abs(dz) == 2) continue;
							place(wx + dx, topY + dy, wz + dz, Leaves);
						}
					}
				}
			}
		}
	}
}
