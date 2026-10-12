#pragma once

#include <bitset>
#include <cstdint>

namespace CGEngine {
	// What a block does. Air is 0 and has no type. The defaults are solid and inert, like the terrain's stone.
	struct BlockType {
		bool solid = true;      // blocks movement (collision and occupancy); a liquid is not solid
		bool falls = false;     // moves down into air, or swaps down with a liquid (sand, gravel)
		bool liquid = false;    // flows (water); see the water levels below
		uint8_t emits = 0;      // light the block gives off at its own cell, 0 to 255 (a lamp is 255); it falls off through air
		// How it is drawn (see the render rules in docs/games/voxels.md). A block is in the chunk's terrain mesh (the default),
		// or in a mesh of its own, one per block type and chunk, drawn with the material set for it. Its shape is the smooth
		// skin, or a cube that never merges with its neighbours, which is what a cube block always is (own is forced).
		bool own = false;       // drawn in its own mesh, not the terrain mesh
		bool cube = false;      // drawn as cubes, even where the terrain is smooth
		uint8_t mesh = 0;       // the mesh of an own block: 0 is the block's own id, else this id, so blocks can share one mesh
		uint8_t joinAbove = 0;  // a smooth block whose top is covered by a block of this mesh joins that mesh (its top only, see skinAt)
		std::bitset<256> joinBelow;  // a smooth block standing on one of these terrain blocks joins the terrain (its bottom only)
		bool transparent = false;    // its mesh is drawn blended, after the opaque ones (the water)
		uint8_t hits = 1;           // how many hits break it (VoxelWorld::hitBlock); a hit on a damaged block shows its stage
	};

	// The mesh a block id is drawn in: 0 for the terrain, else the own mesh (see BlockType::own and mesh).
	inline uint8_t meshGroup(const BlockType& type, uint8_t block) {
		return type.own ? (type.mesh ? type.mesh : block) : 0;
	}

	// Water: block 5 is a full source. Flowing water has a level from 7 down to 1, kept as block ids 9 to 15 (id = 16 -
	// level), so a flow is a block like any other and is saved with the edits. Level 0 is air.
	namespace Water {
		constexpr uint8_t Source = 5;
		constexpr uint8_t FlowFirst = 9;     // level 7, the first level below a source
		constexpr uint8_t FlowLast = 15;     // level 1, the last level a flow reaches
		constexpr int SourceLevel = 8;

		// The level of a liquid block: 8 for the source, 1 to 7 for flowing water, 0 for anything else.
		inline int level(uint8_t block) {
			if (block == Source) return SourceLevel;
			if (block >= FlowFirst && block <= FlowLast) return 16 - block;
			return 0;
		}
		inline uint8_t flowing(int level) { return static_cast<uint8_t>(16 - level); }
	}
}
