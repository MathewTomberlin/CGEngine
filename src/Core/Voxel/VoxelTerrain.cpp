#include <algorithm>
#include <cmath>
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
			}
		}
	}
}
