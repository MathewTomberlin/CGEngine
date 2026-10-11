#pragma once

#include <array>

namespace CGEngine {
	class VoxelWorld;

	// A box that moves through a VoxelWorld's solid blocks: the player, and any body that walks or falls. The C++ version of
	// resources/scripts/VoxelPhysics.py's step, with the same rules, so a body moves identically from either. The box's
	// feet are at pos (y up), and its half width and height are its size. Blocks are unit cubes with corners at integers.
	struct VoxelBodyShape {
		double halfWidth = 0.3;
		double height = 1.8;
		double gravity = 28.0;      // blocks per second squared
		double maxFall = 40.0;      // the fastest fall, in blocks per second
		double substep = 0.1;       // the largest move along one axis in one go, so the box cannot pass through a block
	};

	struct VoxelBodyState {
		std::array<double, 3> pos = { 0.0, 0.0, 0.0 };
		std::array<double, 3> vel = { 0.0, 0.0, 0.0 };
		bool onGround = false;
	};

	// True if the box with its feet at pos touches a solid block.
	bool voxelBodyOverlaps(const VoxelWorld& world, const VoxelBodyShape& shape, const std::array<double, 3>& pos);

	// One step: gravity (unless flying), then the move along x, z and y, each stopped flush against a block it runs into.
	VoxelBodyState stepVoxelBody(const VoxelWorld& world, const VoxelBodyShape& shape, const VoxelBodyState& body, double dt,
		bool flying);
}
