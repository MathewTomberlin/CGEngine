#include <cmath>
#include <algorithm>
#include "VoxelBody.h"
#include "VoxelWorld.h"

namespace CGEngine {
	namespace {
		const double Epsilon = 1e-6;

		// The position on this axis where the box just touches the solid block it ran into, moving in direction.
		double touching(const VoxelBodyShape& shape, double value, int axis, double direction) {
			if (axis == 1) {
				if (direction < 0) return std::floor(value) + 1.0;                                           // feet on the block
				return std::floor(value + shape.height - Epsilon) - shape.height;                            // head under it
			}
			if (direction < 0) return std::floor(value - shape.halfWidth) + 1.0 + shape.halfWidth;          // side against it
			return std::floor(value + shape.halfWidth - Epsilon) - shape.halfWidth;
		}

		// Moves the box along one axis by delta in steps of at most shape.substep. When a step would overlap a solid block,
		// the box is set flush against it and the move stops. Returns true if it was blocked.
		bool moveAxis(const VoxelWorld& world, const VoxelBodyShape& shape, std::array<double, 3>& pos, int axis, double delta) {
			const int steps = std::max(1, static_cast<int>(std::ceil(std::fabs(delta) / shape.substep)));
			const double part = delta / steps;
			for (int s = 0; s < steps; ++s) {
				std::array<double, 3> candidate = pos;
				candidate[axis] += part;
				if (voxelBodyOverlaps(world, shape, candidate)) {
					pos[axis] = touching(shape, candidate[axis], axis, part);
					return true;
				}
				pos = candidate;
			}
			return false;
		}
	}

	bool voxelBodyOverlaps(const VoxelWorld& world, const VoxelBodyShape& shape, const std::array<double, 3>& pos) {
		const int x0 = static_cast<int>(std::floor(pos[0] - shape.halfWidth));
		const int x1 = static_cast<int>(std::floor(pos[0] + shape.halfWidth - Epsilon));
		const int y0 = static_cast<int>(std::floor(pos[1]));
		const int y1 = static_cast<int>(std::floor(pos[1] + shape.height - Epsilon));
		const int z0 = static_cast<int>(std::floor(pos[2] - shape.halfWidth));
		const int z1 = static_cast<int>(std::floor(pos[2] + shape.halfWidth - Epsilon));
		for (int x = x0; x <= x1; ++x) {
			for (int y = y0; y <= y1; ++y) {
				for (int z = z0; z <= z1; ++z) {
					// Cells in chunks that are not loaded count as solid: a body waits at the edge of the loaded world rather
					// than falling through terrain that has not arrived yet.
					if (!world.loadedAt(x, z) || world.isSolid(x, y, z)) return true;
				}
			}
		}
		return false;
	}

	VoxelBodyState stepVoxelBody(const VoxelWorld& world, const VoxelBodyShape& shape, const VoxelBodyState& body, double dt,
		bool flying) {
		VoxelBodyState next = body;
		next.onGround = false;
		if (!flying) next.vel[1] = std::max(-shape.maxFall, next.vel[1] - shape.gravity * dt);
		// Each axis in turn: x, then z, then y. A blocked axis loses its speed; falling onto a block is standing on the ground.
		if (moveAxis(world, shape, next.pos, 0, next.vel[0] * dt)) next.vel[0] = 0.0;
		if (moveAxis(world, shape, next.pos, 2, next.vel[2] * dt)) next.vel[2] = 0.0;
		if (moveAxis(world, shape, next.pos, 1, next.vel[1] * dt)) {
			if (next.vel[1] < 0.0) next.onGround = true;
			next.vel[1] = 0.0;
		}
		return next;
	}
}
