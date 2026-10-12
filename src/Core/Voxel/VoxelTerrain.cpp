#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "VoxelTerrain.h"

namespace CGEngine {
	namespace {
		// A repeatable number in [0, 1] for a lattice point. Same idea as the Python sample's hash.
		float latticeValue(int32_t ix, int32_t iz, uint32_t seed) {
			uint32_t h = static_cast<uint32_t>(ix) * 374761393u + static_cast<uint32_t>(iz) * 668265263u + seed * 1442695041u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return static_cast<float>((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
		}

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

	int VoxelTerrain::columnHeight(int x, int z) const {
		const float n = fractalNoise(static_cast<float>(x), static_cast<float>(z), seed);
		return baseHeight + static_cast<int>(std::pow(n, 1.6f) * static_cast<float>(heightRange));
	}

	VoxelTerrain::Biome VoxelTerrain::biomeAt(int x, int z) const {
		// One noise field over about 90 blocks: the regions are large, so a desert or a forest is a place to walk through.
		const float n = valueNoise(static_cast<float>(x) / 90.0f, static_cast<float>(z) / 90.0f, seed + 4241u);
		if (n > 0.7f) return Biome::Desert;
		if (n < 0.3f) return Biome::Forest;
		return Biome::Plains;
	}

	void VoxelTerrain::fillChunk(int cx, int cz, int size, int height, std::vector<uint8_t>& blocks) const {
		blocks.assign(static_cast<size_t>(size) * size * height, Air);
		for (int lz = 0; lz < size; ++lz) {
			for (int lx = 0; lx < size; ++lx) {
				const int wx = cx * size + lx;
				const int wz = cz * size + lz;
				const int top = std::min(columnHeight(wx, wz), height);
				uint8_t surface = Grass;
				if (top <= seaLevel + 1) surface = Sand;
				else if (top >= snowLevel) surface = Snow;
				else if (biomeAt(wx, wz) == Biome::Desert) surface = Sand;

				// Runs bottom to top: stone, dirt, the surface block, and water above a low column.
				const int stoneTop = std::max(0, top - 4);
				const int dirtTop = std::max(0, top - 1);
				auto fill = [&](int from, int to, uint8_t block) {
					for (int y = std::max(0, from); y < std::min(height, to); ++y) {
						blocks[(static_cast<size_t>(y) * size + lz) * size + lx] = block;
					}
				};
				fill(0, stoneTop, Stone);
				fill(stoneTop, dirtTop, Dirt);
				fill(dirtTop, top, surface);
				if (top < seaLevel) fill(top, seaLevel, Water);

				// Caves are carved below the surface, which stays whole, so they open out only where the noise reaches it.
				if (caveWidth > 0.0f) {
					for (int y = 1; y < std::min(height, top - 2); ++y) {
						if (isCave(cx * size + lx, y, cz * size + lz)) blocks[(static_cast<size_t>(y) * size + lz) * size + lx] = Air;
					}
				}
			}
		}
		placeTrees(cx, cz, size, height, blocks);
	}

	bool VoxelTerrain::isCave(int wx, int wy, int wz) const {
		// Tunnels: the band where two noise fields are both near 0.5 is a network of winding passages. Rooms: rare spots
		// where a coarser field is high.
		const float x = wx / 22.0f, y = wy / 16.0f, z = wz / 22.0f;
		const float a = valueNoise3(x, y, z, seed + 5003u) - 0.5f;
		const float b = valueNoise3(x + 17.3f, y + 9.1f, z + 4.7f, seed + 7019u) - 0.5f;
		if (std::fabs(a) < caveWidth && std::fabs(b) < caveWidth) return true;
		return valueNoise3(wx / 40.0f, wy / 30.0f, wz / 40.0f, seed + 9011u) > 0.80f;
	}

	// A tree stands on a grass column, and whether it does is a hash of the column, so every chunk that sees the tree draws
	// the same one. A trunk of 4 to 6 blocks ends in a crown: two layers of radius 2 (corners cut), one of radius 1 around
	// the trunk top, and a tip.
	void VoxelTerrain::placeTrees(int cx, int cz, int size, int height, std::vector<uint8_t>& blocks) const {
		// The crown reaches 2 blocks from the trunk, so the trees rooted up to 3 blocks outside the chunk can reach into it.
		const int margin = 3;
		auto place = [&](int wx, int y, int wz, uint8_t block) {
			const int lx = wx - cx * size;
			const int lz = wz - cz * size;
			if (lx < 0 || lx >= size || lz < 0 || lz >= size || y < 0 || y >= height) return;
			uint8_t& cell = blocks[(static_cast<size_t>(y) * size + lz) * size + lx];
			if (cell == Air) cell = block;
		};
		for (int wz = cz * size - margin; wz < (cz + 1) * size + margin; ++wz) {
			for (int wx = cx * size - margin; wx < (cx + 1) * size + margin; ++wx) {
				// Forests grow about three times the trees of the plains, and deserts none.
				const Biome biome = biomeAt(wx, wz);
				if (biome == Biome::Desert) continue;
				const float chance = biome == Biome::Forest ? treeChance * 3.0f : treeChance;
				if (latticeValue(wx, wz, seed + 9001u) >= chance) continue;
				const int ground = columnHeight(wx, wz);
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
