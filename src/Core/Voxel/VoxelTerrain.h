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
		static constexpr uint8_t EndlessWater = 31;   // the sea is endless water: it spills its flows as before, and makes no volume (see VoxelBlocks.h)
		static constexpr uint8_t Snow = 6;
		static constexpr uint8_t Wood = 7;
		static constexpr uint8_t Leaves = 8;
		static constexpr uint8_t Lamp = 17;     // a full block that gives off light (see BlockType::emits); defined in VoxelGen.py
		static constexpr uint8_t Coal = 20;     // ore in the stone: common, at any depth
		static constexpr uint8_t Iron = 21;     // ore in the stone: rare, deep only (see fillChunk)
		static constexpr uint8_t Lava = 22;     // a producing lava spring (defined in VoxelGen.py); the caves do not use it
		static constexpr uint8_t EndlessLava = 32;   // the cave lava: an endless spring, so the caves at or below LavaLevel do not fill with it

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

		// The biome of column (x, z): slow noise picks the sea (a basin under the sea level, with sand and water), desert (sand on top,
		// no trees), forest (many trees) or plains.
		enum class Biome { Plains, Forest, Desert, Sea };
		Biome biomeAt(int x, int z) const;
		// How much of the sea column (x, z) is, from 0 (land) to 1 (the deep sea floor). The sea's shore is the range in between.
		float seaWeight(int x, int z) const;

		// Fills blocks for section (cx, cy, cz): size cubed blocks, block (x, y, z) at (ly * size + lz) * size + lx, where ly is y minus the
		// section's floor (cy * size). Each block is a function of its world position, so every section agrees on the blocks it shares.
		void fillSection(int cx, int cy, int cz, int size, std::vector<uint8_t>& blocks, bool features = true) const;
		// A column's top (its columnHeight) and its surface block: what a cell's terrain depends on besides its height.
		struct Column {
			int top = 0;
			uint8_t surface = 1;
		};
		Column columnAt(int wx, int wz) const;
		// Floating islands (roadmap 45.2): blobs of stone, dirt and grass in the air between IslandMin and IslandMax, above the ground.
		// Whether an island is at (wx, wy, wz): a slow field picks the regions that have islands, and a 3D field shapes each one.
		bool islandAt(int wx, int wy, int wz) const;
		// How much of the sky is cloud at column (wx, wz) at time t (seconds), 0 to 1: a slow field that drifts across the world.
		float cloudCover(int wx, int wz, double t) const;
		// The terrain block of one cell, from its column, with no trees, rocks or edits: the same block fillSection gives it, at any depth.
		uint8_t cellBlock(int wx, int wy, int wz, const Column& column) const;
		// The heights floating islands can be at (blocks): above the highest ground (about 58) and inside the loaded band of a player
		// near the ground.
		static constexpr int IslandMin = 60;
		static constexpr int IslandMax = 76;
		// Fills blocks for column (cx, cz): size * size * height blocks, block (x, y, z) at (y * size + z) * size + x. It is the sections of
		// the column, stacked (fillSection).
		void fillChunk(int cx, int cz, int size, int height, std::vector<uint8_t>& blocks) const;

	private:
		// Trees and boulders are drawn into air only, in the section's cells. A tree or boulder rooted outside the section reaches in and is
		// drawn here too, as the same shape the section that holds its root draws.
		void placeTrees(int cx, int cy, int cz, int size, std::vector<uint8_t>& blocks) const;
		// Boulders of stone on the ground: a hash of the column decides, by biome, so every section that sees one draws the same.
		void placeRocks(int cx, int cy, int cz, int size, std::vector<uint8_t>& blocks) const;
		// Whether the block at world (x, y, z) is carved out as cave: a tunnel where two noise fields are both near their middle,
		// or a room where a third is high.
		bool isCave(int wx, int wy, int wz) const;
		// Whether a cave (or lava in one) is beside the block at (wx, wy, wz) (the ores in the stone beside a cave).
		bool besideCave(int wx, int wy, int wz) const;
		// A column's height and biome, remembered per thread (the trees and rocks read the columns around a section, and each section of a column
		// reads the same ones). Kept per settings, so a terrain with other settings is never given these.
		struct ColumnMemo {
			uint32_t seed;
			int seaLevel;
			int baseHeight;
			int heightRange;
			int snowLevel;
			int top;
			uint8_t biome;
		};
		ColumnMemo memoColumn(int wx, int wz) const;
	};
}
