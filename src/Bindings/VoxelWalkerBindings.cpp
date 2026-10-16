#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <SFML/System/Vector3.hpp>
#include "../Core/Voxel/VoxelWalker.h"
#include "../Core/Voxel/VoxelWorld.h"
#include "../Core/Body/Body.h"
#include "../Core/Mesh/Mesh.h"
#include "../Core/Scripts/Script.h"
#include "../Core/Engine/Engine.h"
#include "../Core/Engine/EngineSystem.h"

namespace py = pybind11;
using namespace py::literals;
using namespace CGEngine;

namespace {
	// A walker and the id of the update script that moves its body, by the body's name.
	struct WalkerEntry {
		std::shared_ptr<VoxelWalker> walker;
		id_t scriptId = 0;
	};
	std::map<std::string, WalkerEntry> walkers;

	Body* bodyNamed(const std::string& name) {
		for (Body* body : assets.getAllResources<Body>()) {
			if (body->getName() == name) return body;
		}
		return nullptr;
	}

	VoxelWalker& walkerNamed(const std::string& name) {
		auto found = walkers.find(name);
		if (found == walkers.end()) throw std::runtime_error("no voxel walker on a body named '" + name + "'");
		return *found->second.walker;
	}

	// The body follows the walker: its mesh is placed at the walker's feet.
	void placeBody(Body* body, const VoxelBodyState& state) {
		if (!body) return;
		if (Mesh* mesh = body->get<Mesh*>()) {
			Vector3f position = { static_cast<float>(state.pos[0]), static_cast<float>(state.pos[1]), static_cast<float>(state.pos[2]) };
			mesh->setPosition(position);
		}
	}

	void removeWalker(const std::string& name) {
		auto found = walkers.find(name);
		if (found == walkers.end()) return;
		if (Body* body = bodyNamed(name)) body->removeScript(onUpdateEvent, found->second.scriptId, true);
		walkers.erase(found);
	}
}

// Voxel walkers: a mesh body moved by the voxel mover each update (src/Core/Voxel/VoxelWalker.h). Scripts set the walking
// velocity and jumps; the engine does the gravity and the collisions.
void bindVoxelWalkers(py::module_& m) {
	m.def("add_voxel_walker", [](const std::string& name, const VoxelWorld& world, double halfWidth, double height) {
		Body* body = bodyNamed(name);
		if (!body) throw std::runtime_error("no body named '" + name + "'");
		Mesh* mesh = body->get<Mesh*>();
		if (!mesh) throw std::runtime_error("body '" + name + "' is not a mesh");
		removeWalker(name);
		auto walker = std::make_shared<VoxelWalker>();
		walker->world = &world;
		walker->shape.halfWidth = halfWidth;
		walker->shape.height = height;
		const Transformation3D placed = mesh->getTransformation();
		walker->state.pos = { placed.position.x, placed.position.y, placed.position.z };
		// The frame's time is capped as the game caps it, so a slow frame does not let a walker tunnel through a block.
		const id_t scriptId = body->addUpdateScript(new Script([walker](ScArgs args) {
			walker->step(std::min(static_cast<double>(CGEngine::time.getDeltaSec()), 0.05));
			placeBody(args.caller, walker->state);
		}));
		walkers[name] = WalkerEntry{ walker, scriptId };
	}, py::arg("name"), py::arg("world"), py::arg("half_width") = 0.3, py::arg("height") = 1.8,
	   "Make the mesh body called name a walker on this world: each update it falls, lands and stops against blocks, and it "
	   "walks at the velocity set by set_voxel_velocity. The world must stay alive until remove_voxel_walker.");

	m.def("remove_voxel_walker", &removeWalker, py::arg("name"), "Stop the walker on a body. The body stays where it is.");

	m.def("set_voxel_velocity", [](const std::string& name, double vx, double vz) {
		VoxelWalker& walker = walkerNamed(name);
		walker.state.vel[0] = vx;
		walker.state.vel[2] = vz;
	}, py::arg("name"), py::arg("vx"), py::arg("vz"), "Set the walker's horizontal velocity (blocks per second). Gravity is the engine's.");

	m.def("voxel_jump", [](const std::string& name, double speed) {
		VoxelWalker& walker = walkerNamed(name);
		if (!walker.state.onGround) return false;
		walker.state.vel[1] = speed;
		return true;
	}, py::arg("name"), py::arg("speed"), "Jump (upwards, blocks per second) if the walker is on the ground. Returns whether it jumped.");

	m.def("voxel_walker_state", [](const std::string& name) {
		const VoxelWalker& walker = walkerNamed(name);
		const auto& p = walker.state.pos;
		const auto& v = walker.state.vel;
		return py::make_tuple(py::make_tuple(p[0], p[1], p[2]), py::make_tuple(v[0], v[1], v[2]), walker.state.onGround);
	}, py::arg("name"), "The walker's feet position, its velocity, and whether it stands on the ground: (pos, vel, on_ground).");

	m.def("set_voxel_position", [](const std::string& name, double x, double y, double z) {
		VoxelWalker& walker = walkerNamed(name);
		walker.state.pos = { x, y, z };
		walker.state.vel = { 0.0, 0.0, 0.0 };
		walker.state.onGround = false;
		placeBody(bodyNamed(name), walker.state);
	}, py::arg("name"), py::arg("x"), py::arg("y"), py::arg("z"), "Put the walker's feet at (x, y, z) and stop it.");

	m.def("step_voxel_walker", [](const std::string& name, double dt) {
		VoxelWalker& walker = walkerNamed(name);
		walker.step(dt);
		placeBody(bodyNamed(name), walker.state);
	}, py::arg("name"), py::arg("dt"), "Move the walker by dt seconds now, as its update does. For tests that run without frames.");
}
