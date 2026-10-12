#pragma once

#include "VoxelBody.h"

namespace CGEngine {
	class VoxelWorld;

	// A body that the voxel mover moves once per update: gravity, collision with the solid blocks (cells of chunks that are
	// not loaded count as solid), and a horizontal velocity that a script sets. A script attaches one to a mesh body with
	// cge.add_voxel_walker, and the body's position follows the walker's feet. The world must outlive the walker: remove the
	// walker (cge.remove_voxel_walker) before the world is destroyed.
	struct VoxelWalker {
		const VoxelWorld* world = nullptr;
		VoxelBodyShape shape;
		VoxelBodyState state;

		// Moves the walker for dt seconds, the same step the update script takes.
		void step(double dt);
	};
}
