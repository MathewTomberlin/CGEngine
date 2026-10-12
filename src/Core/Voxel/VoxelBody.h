#pragma once

#include <array>

namespace CGEngine {
	class VoxelWorld;

	// A box that moves over the voxel terrain: the player, and any body that walks or falls. It collides with the skin the
	// terrain is drawn with (VoxelWorld::skinDensity), not with the blocks under it, so a body stands on the visible ground,
	// walks up gentle slopes, and cannot push its eye into a wall. The C++ version of resources/scripts/VoxelPhysics.py's
	// step, with the same rules, so a body moves identically from either. The feet are at pos (y up).
	struct VoxelBodyShape {
		double halfWidth = 0.3;
		double height = 1.8;
		double eye = 1.6;           // the eye is a point of the body, so the camera stays outside the skin
		double gravity = 28.0;      // blocks per second squared
		double maxFall = 40.0;      // the fastest fall, in blocks per second
		double substep = 0.1;       // the largest move along one axis in one go
		double stepUp = 1.0;        // a blocked walk on the ground climbs this high (a block), then snaps back to the surface
	};

	struct VoxelBodyState {
		std::array<double, 3> pos = { 0.0, 0.0, 0.0 };
		std::array<double, 3> vel = { 0.0, 0.0, 0.0 };
		bool onGround = false;
	};

	// True if any point of the body (its feet, middle, head, eye) is inside the skin.
	bool voxelBodyInside(const VoxelWorld& world, const VoxelBodyShape& shape, const std::array<double, 3>& pos);

	// One step: gravity (unless flying), then the walk along x and z (climbing a step where the ground rises), then the move
	// along y. Every blocked move is set flush against the surface it ran into.
	VoxelBodyState stepVoxelBody(const VoxelWorld& world, const VoxelBodyShape& shape, const VoxelBodyState& body, double dt,
		bool flying);
}
