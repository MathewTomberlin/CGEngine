#pragma once

#include <cstdint>
#include <vector>

namespace CGEngine {
	// The terrain generator of the voxel world: smooth noise gives each column a height, and each column is stacked
	// stone, dirt and a surface block, with water up to sea level. It is a pure function of the settings and the chunk
	// position, so chunks can be generated on any thread.
	struct VoxelTerrain {
		// Block ids. Air is 0. The Python side uses the same numbers for its colours (resources/scripts/VoxelGen.py).
		static constexpr uint8_t Air = 0;
		static constexpr uint8_t Grass = 1;
		static constexpr uint8_t Dirt = 2;
		static constexpr uint8_t Stone = 3;
		static constexpr uint8_t Sand = 4;
		static constexpr uint8_t Water = 5;
		static constexpr uint8_t Snow = 6;

		uint32_t seed = 1;
		int seaLevel = 14;      // columns below this are filled with water up to it
		int baseHeight = 8;     // the lowest column is this high (in blocks)
		int heightRange = 44;   // a column is at most baseHeight + heightRange high
		int snowLevel = 40;     // columns at or above this are capped with snow

		// Height of the column at world (x, z), in blocks, from baseHeight up to baseHeight + heightRange.
		int columnHeight(int x, int z) const;

		// Fills blocks for chunk (cx, cz): size * size * height blocks, block (x, y, z) at (y * size + z) * size + x.
		void fillChunk(int cx, int cz, int size, int height, std::vector<uint8_t>& blocks) const;
	};
}
