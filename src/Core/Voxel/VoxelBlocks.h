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
		bool blend = false;          // its mesh is drawn blended, after the opaque ones (the water)
		bool passesLight = false;    // sky and lamp light go through it (air, and the water)
		uint8_t hits = 1;           // how many hits break it (VoxelWorld::hitBlock); a hit on a damaged block shows its stage
		bool flows = true;          // a liquid spreads as water does; false keeps it where it is (lava)
		// A textured block shows the voxel tile array (Renderer::setVoxelTiles) instead of its colour: one layer for its top,
		// its sides and its bottom. Its vertex colour is white, so the tile is the colour.
		bool textured = false;
		uint8_t tiles[3] = { 0, 0, 0 };   // the tile layers of the top, the sides and the bottom (when textured)
		// A terrain block's splat channel, 0 to 15: its weight is blended per pixel from the splat tiles of the smooth skin. -1 for a
		// block that is not a splat block (it takes its own tiles per vertex).
		int8_t splat = -1;
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

	// A liquid's blocks: its source (a full block), and its flowing levels from 7 down to 1 as block ids first to last, with
	// id = top - level. Water and lava flow the same way (VoxelSimulation).
	struct Liquid {
		uint8_t source;
		uint8_t first;
		uint8_t last;
		int top;
		// The level of a block of this liquid: 8 for the source, 1 to 7 for flowing, 0 for anything else.
		int level(uint8_t block) const {
			if (block == source) return 8;
			if (block >= first && block <= last) return top - block;
			return 0;
		}
		uint8_t flowing(int level) const { return static_cast<uint8_t>(top - level); }
	};
	inline const Liquid WaterFlow{ Water::Source, Water::FlowFirst, Water::FlowLast, 16 };
	inline const Liquid LavaFlow{ 22, 23, 29, 30 };   // lava is block 22 (VoxelTerrain::Lava); its flows are 23 to 29 (VoxelGen.py)
	// The liquid a block is, or nullptr for a block that is not a liquid.
	inline const Liquid* liquidOf(uint8_t block) {
		if (WaterFlow.level(block) > 0) return &WaterFlow;
		if (LavaFlow.level(block) > 0) return &LavaFlow;
		return nullptr;
	}

	// A volume: a placed amount of liquid, kept as block ids. Unit u (1 to 8) of a family is block first + u - 1, so a full volume
	// is 8 units. Springs (above) make flows from a source that never runs out; a volume never makes or loses units, it only moves
	// them (VoxelSimulation::tickVolume), so it spreads out and levels off. The ids are above the item ids (VoxelGen.py).
	struct VolumeLiquid {
		uint8_t first;   // the block of one unit
		uint8_t spring;  // the spring block whose mesh the volume is drawn in (water 5, lava 22)
		static constexpr int Full = 8;
		// The units a block holds in this family, 0 for any other block.
		int units(uint8_t block) const {
			return (block >= first && block < first + Full) ? block - first + 1 : 0;
		}
		uint8_t block(int u) const { return static_cast<uint8_t>(first + u - 1); }
	};
	inline const VolumeLiquid WaterVolume{ 128, Water::Source };
	inline const VolumeLiquid LavaVolume{ 136, 22 };
	// The volume a block is part of, or nullptr for a block that is not in one.
	inline const VolumeLiquid* volumeOf(uint8_t block) {
		if (WaterVolume.units(block) > 0) return &WaterVolume;
		if (LavaVolume.units(block) > 0) return &LavaVolume;
		return nullptr;
	}
}
