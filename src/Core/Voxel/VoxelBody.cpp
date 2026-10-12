#include <algorithm>
#include <cmath>
#include "VoxelBody.h"
#include "VoxelWorld.h"
#include "VoxelMesher.h"

namespace CGEngine {
	namespace {
		using Vec3 = std::array<double, 3>;

		// The points of the body that must stay outside the skin: five across at each height (the four corners and the middle),
		// at the feet, the middle, the head, and the eye.
		bool insideAt(const VoxelWorld& world, const VoxelBodyShape& shape, const Vec3& pos) {
			const double hw = shape.halfWidth;
			const double heights[4] = { pos[1], pos[1] + shape.height * 0.5, pos[1] + shape.height - 0.02, pos[1] + shape.eye };
			const double across[5][2] = { { -hw, -hw }, { hw, -hw }, { -hw, hw }, { hw, hw }, { 0.0, 0.0 } };
			for (double y : heights) {
				for (const auto& offset : across) {
					if (world.skinDensity(pos[0] + offset[0], y, pos[2] + offset[1]) > VoxelSkinIso) return true;
				}
			}
			return false;
		}

		// Moves the body along one axis by delta, in steps of at most shape.substep. When a step would enter the skin, the
		// body is set flush against it by bisection (the surface is not on a grid, so it is found, not snapped). Returns true
		// if the move was blocked.
		bool moveAxis(const VoxelWorld& world, const VoxelBodyShape& shape, Vec3& pos, int axis, double delta) {
			const int steps = std::max(1, static_cast<int>(std::ceil(std::fabs(delta) / shape.substep)));
			const double part = delta / steps;
			for (int s = 0; s < steps; ++s) {
				Vec3 candidate = pos;
				candidate[axis] += part;
				if (insideAt(world, shape, candidate)) {
					double free = 0.0;
					double blocked = 1.0;
					for (int i = 0; i < 12; ++i) {
						const double mid = (free + blocked) * 0.5;
						Vec3 probe = pos;
						probe[axis] += part * mid;
						if (insideAt(world, shape, probe)) {
							blocked = mid;
						} else {
							free = mid;
						}
					}
					pos[axis] += part * free;
					return true;
				}
				pos = candidate;
			}
			return false;
		}

		// Written as the Python copy writes it (VoxelPhysics.py), so the two agree to the last digit.
		double horizontalDistance(const Vec3& a, const Vec3& b) {
			const double dx = b[0] - a[0], dz = b[2] - a[2];
			return std::sqrt(dx * dx + dz * dz);
		}
	}

	bool voxelBodyInside(const VoxelWorld& world, const VoxelBodyShape& shape, const Vec3& pos) {
		return insideAt(world, shape, pos);
	}

	VoxelBodyState stepVoxelBody(const VoxelWorld& world, const VoxelBodyShape& shape, const VoxelBodyState& body, double dt,
		bool flying) {
		VoxelBodyState next = body;
		next.onGround = false;
		if (!flying) next.vel[1] = std::max(-shape.maxFall, next.vel[1] - shape.gravity * dt);
		// A body that starts inside the skin (it was placed there, or the ground rose under it) is lifted out first.
		Vec3 start = next.pos;
		for (int i = 0; i < 40 && insideAt(world, shape, start); ++i) start[1] += 0.05;

		// The walk. First straight on, x then z. If that is blocked on the ground (and not rising), try again from a step higher,
		// and keep that walk if it went further. Then snap back down to whatever surface is under the new place.
		Vec3 flat = start;
		bool blockedX = moveAxis(world, shape, flat, 0, next.vel[0] * dt);
		bool blockedZ = moveAxis(world, shape, flat, 2, next.vel[2] * dt);
		Vec3 chosen = flat;
		bool steppedOnGround = false;
		if ((blockedX || blockedZ) && body.onGround && !flying && body.vel[1] <= 0.0) {
			Vec3 raised = start;
			raised[1] += shape.stepUp;
			if (!insideAt(world, shape, raised)) {
				Vec3 up = raised;
				const bool upX = moveAxis(world, shape, up, 0, next.vel[0] * dt);
				const bool upZ = moveAxis(world, shape, up, 2, next.vel[2] * dt);
				const bool snapped = moveAxis(world, shape, up, 1, -(shape.stepUp + 0.05));
				if (horizontalDistance(start, up) > horizontalDistance(start, flat)) {
					chosen = up;
					blockedX = upX;
					blockedZ = upZ;
					steppedOnGround = snapped;
				}
			}
		}
		next.pos = chosen;
		if (blockedX) next.vel[0] = 0.0;
		if (blockedZ) next.vel[2] = 0.0;
		if (steppedOnGround) next.onGround = true;

		// The fall or the jump. Landing (a blocked move downwards) is standing on the ground.
		const bool blockedY = moveAxis(world, shape, next.pos, 1, next.vel[1] * dt);
		if (blockedY) {
			if (next.vel[1] < 0.0) next.onGround = true;
			next.vel[1] = 0.0;
		}
		return next;
	}
}
