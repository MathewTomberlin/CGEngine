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
		static constexpr uint8_t Wood = 7;
		static constexpr uint8_t Leaves = 8;
		static constexpr uint8_t Lamp = 17;     // a full block that gives off light (see BlockType::emits); defined in VoxelGen.py
		static constexpr uint8_t Coal = 20;     // ore in the stone: common, at any depth
		static constexpr uint8_t Iron = 21;     // ore in the stone: rare, deep only (see fillChunk)
		static constexpr uint8_t Lava = 22;     // a liquid that does not flow: the caves at or below LavaLevel are full of it (defined in VoxelGen.py)

		uint32_t seed = 1;
		int seaLevel = 14;      // columns below this are filled with water up to it
		int baseHeight = 8;     // the lowest column is this high (in blocks)
		int heightRange = 44;   // a column is at most baseHeight + heightRange high
		int snowLevel = 40;     // columns at or above this are capped with snow
		float rockChance = 0.0015f; // the chance that a column holds a boulder on the plains (forests a half, deserts twice): about one a chunk in the desert
		float treeChance = 0.01f; // the chance that a grass column grows a tree (about two or three a chunk)
		float caveWidth = 0.07f;  // how wide the tunnels are, as a band of the noise (0 turns caves off); rooms come with them

		// Height of the column at world (x, z), in blocks, from baseHeight up to baseHeight + heightRange.
		int columnHeight(int x, int z) const;

		// The biome of column (x, z): slow noise picks desert (sand on top, no trees), forest (many trees) or plains.
		enum class Biome { Plains, Forest, Desert };
		Biome biomeAt(int x, int z) const;

		// Fills blocks for chunk (cx, cz): size * size * height blocks, block (x, y, z) at (y * size + z) * size + x.
		// Trees are drawn last, into air only; the trees rooted just outside the chunk reach in and are drawn here too.
		void fillChunk(int cx, int cz, int size, int height, std::vector<uint8_t>& blocks) const;

	private:
		void placeTrees(int cx, int cz, int size, int height, std::vector<uint8_t>& blocks) const;
		// Boulders of stone on the ground: a hash of the column decides, by biome, so every chunk that sees one draws the same.
		void placeRocks(int cx, int cz, int size, int height, std::vector<uint8_t>& blocks) const;
		// Whether the block at world (x, y, z) is carved out as cave: a tunnel where two noise fields are both near their middle,
		// or a room where a third is high.
		bool isCave(int wx, int wy, int wz) const;
	};
}
