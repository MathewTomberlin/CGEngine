#pragma once

#include <cstdint>

namespace CGEngine {
	// What a block does. Air is 0 and has no type. The defaults are solid and inert, like the terrain's stone.
	struct BlockType {
		bool solid = true;      // blocks movement (collision and occupancy); a liquid is not solid
		bool falls = false;     // moves down into air, or swaps down with a liquid (sand, gravel)
		bool liquid = false;    // flows (water); see the water levels below
	};

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
