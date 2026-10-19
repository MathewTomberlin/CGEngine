#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <SFML/System/Vector3.hpp>
#include <optional>
#include "../Core/Voxel/VoxelHerd.h"
#include "../Core/Voxel/VoxelWalker.h"
#include "../Core/Voxel/VoxelWorld.h"
#include "../Core/Body/Body.h"
#include "../Core/Mesh/Mesh.h"
#include "../Core/Mesh/RuntimeMesh.h"
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
		return findBodyByName(name);
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

	// The herd's numbers and a kind's numbers, from the dicts the game sends (resources/scripts/VoxelWalkers.py).
	double number(const py::dict& d, const char* key) {
		return d[py::str(key)].cast<double>();
	}

	double numberOr(const py::dict& d, const char* key, double fallback) {
		return d.contains(py::str(key)) && !d[py::str(key)].is_none() ? d[py::str(key)].cast<double>() : fallback;
	}

	HerdRules rulesFrom(const py::dict& d) {
		HerdRules r;
		r.respawnDepth = number(d, "respawn_depth");
		r.bumpRange = number(d, "bump_range");
		r.lookAhead = number(d, "look_ahead");
		r.knockSeconds = number(d, "knock_seconds");
		r.knockPace = number(d, "knock_pace");
		r.flashSeconds = number(d, "flash_seconds");
		r.hurtSeconds = number(d, "hurt_seconds");
		r.dieSeconds = number(d, "die_seconds");
		r.shieldCos = number(d, "shield_cos");
		r.flankSeconds = number(d, "flank_seconds");
		r.flankTurn = number(d, "flank_turn");
		r.attackSeconds = number(d, "attack_seconds");
		r.biteWindup = number(d, "bite_windup");
		r.routeSeconds = number(d, "route_seconds");
		r.routeSearchesPerFrame = static_cast<int>(number(d, "route_searches_per_frame"));
		r.routeReach = number(d, "route_reach");
		r.contact = number(d, "contact");
		r.contactSlack = number(d, "contact_slack");
		r.jumpSpeed = number(d, "jump_speed");
		r.playerHeight = number(d, "player_height");
		r.halfWidth = number(d, "half_width");
		r.creatureHeight = number(d, "creature_height");
		r.lavaInterval = number(d, "lava_interval");
		r.chaseRange = number(d, "chase_range");
		r.nightChaseShare = number(d, "night_chase_share");
		r.spawnMin = numberOr(d, "spawn_min", r.spawnMin);
		r.spawnMax = numberOr(d, "spawn_max", r.spawnMax);
		r.despawnRange = numberOr(d, "despawn_range", r.despawnRange);
		r.interactRange = numberOr(d, "interact_range", r.interactRange);
		r.spacing = numberOr(d, "spacing", r.spacing);
		r.lookCos = numberOr(d, "look_cos", r.lookCos);
		r.chunk = static_cast<int>(numberOr(d, "chunk", r.chunk));
		r.seaLevel = static_cast<int>(numberOr(d, "sea_level", r.seaLevel));
		r.worldHeight = static_cast<int>(numberOr(d, "world_height", r.worldHeight));
		r.lava = d[py::str("lava")].cast<std::vector<int>>();
		r.liquids = d[py::str("liquids")].cast<std::vector<int>>();
		return r;
	}

	HerdKind kindFrom(const py::dict& d) {
		HerdKind k;
		k.bites = d[py::str("bites")].cast<bool>();
		k.health = number(d, "health");
		k.pace = number(d, "pace");
		k.chasePace = numberOr(d, "chase_pace", k.chasePace);
		k.fleePace = numberOr(d, "flee_pace", k.fleePace);
		k.fleeRange = numberOr(d, "flee_range", 0.0);
		k.biteRange = numberOr(d, "bite_range", k.biteRange);
		k.biteDamage = numberOr(d, "bite_damage", k.biteDamage);
		k.biteInterval = numberOr(d, "bite_interval", k.biteInterval);
		k.preyRange = numberOr(d, "prey_range", 0.0);
		k.sight = numberOr(d, "sight", -1.0);
		k.fightBackSeconds = numberOr(d, "fight_back_seconds", -1.0);
		k.material = d[py::str("material")].cast<std::string>();
		k.flashMaterial = d[py::str("flash_material")].cast<std::string>();
		// The clips by role (herd_kind in VoxelWalkers.py puts them in a "clips" dict): a role the dict does not name has no clip.
		if (d.contains(py::str("clips")) && !d[py::str("clips")].is_none()) {
			py::dict clips = d[py::str("clips")].cast<py::dict>();
			const char* roles[] = { "walk", "bound", "attack", "hurt", "die" };
			for (size_t i = 0; i < 5; ++i) {
				if (clips.contains(py::str(roles[i]))) k.clips[i] = clips[py::str(roles[i])].cast<std::string>();
			}
		}
		return k;
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

	// The creature herd (src/Core/Voxel/VoxelHerd.h): every creature's decisions and moves run in C++ each frame.
	py::class_<VoxelHerd>(m, "VoxelHerd")
		.def(py::init([](const VoxelWorld& world, py::dict rules, uint32_t seed) {
			return std::make_unique<VoxelHerd>(world, rulesFrom(rules), seed);
		}), py::arg("world"), py::arg("rules"), py::arg("seed"),
			"A herd on this world, with the game's rules (a dict of numbers and the lava and liquid blocks). The world must outlive the herd.")
		.def("add", [](VoxelHerd& herd, const std::string& name, std::array<double, 3> spawn, py::dict kind) {
			herd.add(name, spawn, kindFrom(kind));
		}, py::arg("name"), py::arg("spawn"), py::arg("kind"), "Add a creature on a mesh body of that name, its feet at spawn.")
		.def("update", [](VoxelHerd& herd, double dt, std::optional<std::array<double, 2>> player, std::optional<double> playerFeet,
				double night, std::optional<std::array<double, 2>> shield) {
			VoxelHerd::Result result = herd.update(dt, player ? &*player : nullptr, playerFeet ? &*playerFeet : nullptr, night,
				shield ? &*shield : nullptr);
			return py::dict("bites"_a = result.bites, "blocks"_a = result.blocks, "killed"_a = result.killed);
		}, py::arg("dt"), py::arg("player") = py::none(), py::arg("player_feet") = py::none(), py::arg("night") = 0.0,
			py::arg("shield") = py::none(),
			"Move every creature for dt seconds. Returns bites (the health the player loses), blocks (shield blocks) and killed (names of "
			"the creatures that died this frame, by a bite or by lava).")
		.def("hurt", [](VoxelHerd& herd, const std::string& name, std::array<double, 2> from, double damage, bool provoked) {
			return herd.hurt(name, from, damage, provoked);
		}, py::arg("name"), py::arg("from_xz"), py::arg("damage") = 1.0, py::arg("provoked") = true,
			"A hit from (x, z): True when it kills the creature (the caller then calls die).")
		.def("die", &VoxelHerd::die, py::arg("name"), "The creature dies: it stops, plays its die clip, then its body goes.")
		.def("alive", &VoxelHerd::alive, py::arg("name"), "Whether the creature exists and is not dying.")
		.def("health", &VoxelHerd::health, py::arg("name"), "The creature's health.")
		.def("state", [](const VoxelHerd& herd, const std::string& name) {
			const VoxelBodyState s = herd.state(name);
			return py::make_tuple(py::make_tuple(s.pos[0], s.pos[1], s.pos[2]), py::make_tuple(s.vel[0], s.vel[1], s.vel[2]), s.onGround);
		}, py::arg("name"), "The creature's feet, velocity and ground contact: (pos, vel, on_ground).")
		.def("count", &VoxelHerd::count, "The creatures in the herd, dying ones included.")
		.def("live_count", &VoxelHerd::liveCount, "The creatures that are alive.")
		.def("find_spawn", [](VoxelHerd& herd, std::array<double, 2> player, double feet, std::optional<std::array<double, 2>> look,
				int attempts, double minDistance, double maxDistance) -> py::object {
			auto spot = herd.findSpawn(player, feet, look ? &*look : nullptr, attempts, minDistance, maxDistance);
			if (!spot) return py::none();
			return py::make_tuple((*spot)[0], (*spot)[1], (*spot)[2]);
		}, py::arg("player"), py::arg("feet"), py::arg("look") = py::none(), py::arg("attempts") = 16,
			py::arg("min_distance") = 16.0, py::arg("max_distance") = 32.0,
			"A spawn spot (x, y, z) for a creature, or None: out of view when look (the player's horizontal look) is given, on loaded ground above "
			"the water, and not close to a creature.")
		.def("despawn", [](VoxelHerd& herd, std::array<double, 2> player, double feet, std::vector<std::string> keep, bool arrowsFlying,
				int target) {
			return herd.despawn(player, feet, keep, arrowsFlying, target);
		}, py::arg("player"), py::arg("feet"), py::arg("keep"), py::arg("arrows_flying"), py::arg("target"),
			"Take out the creatures nothing holds that are beyond the despawn range (and the farthest one while over target). Returns their "
			"names; their bodies go with the Remove events. Nothing goes while an arrow flies.")
		.def("heading", &VoxelHerd::heading, py::arg("name"), "The way the creature is heading, in radians (for the checks).")
		.def("set_heading", &VoxelHerd::setHeading, py::arg("name"), py::arg("heading"), py::arg("timer"),
			"Turn the creature to a heading, and set its wander timer (for the checks).")
		.def("set_position", [](VoxelHerd& herd, const std::string& name, double x, double y, double z) { herd.setPosition(name, x, y, z); }, py::arg("name"), py::arg("x"), py::arg("y"), py::arg("z"),
			"Put the creature's feet at (x, y, z) and stop it (for the checks and the console).")
		.def("take_events", [](VoxelHerd& herd) {
			py::list out;
			for (const HerdEvent& e : herd.takeEvents()) {
				const char* kind = e.kind == HerdEvent::Kind::Clip ? "clip" : e.kind == HerdEvent::Kind::Material ? "material" : "remove";
				out.append(py::make_tuple(kind, e.name, e.value, e.looping));
			}
			return out;
		}, "What the game must do to bodies since the last call: (kind, name, value, looping), kind being clip (play value, looping), "
			"material (draw value) or remove (take the body out).");
}
