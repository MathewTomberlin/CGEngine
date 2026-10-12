#include "VoxelWalker.h"
#include "VoxelWorld.h"

namespace CGEngine {
	void VoxelWalker::step(double dt) {
		state = world->stepBody(state, shape, dt, false);
	}
}
