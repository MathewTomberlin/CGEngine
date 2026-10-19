#include "VoxelHerd.h"
#include "VoxelWorld.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <SFML/System/Vector3.hpp>
#include "../Body/Body.h"
#include "../Mesh/Mesh.h"
#include "../Mesh/RuntimeMesh.h"

namespace CGEngine {
	namespace {
		// The clip roles, by index into HerdKind::clips.
		enum Role { Walk = 0, Bound = 1, Attack = 2, Hurt = 3, Die = 4 };
		const double PI = 3.14159265358979323846;
		const double MaxPhysicsStep = 0.05;   // the largest physics step, as the game caps the frame (VoxelWalker)

		double clamp01(double v) {
			return std::min(1.0, std::max(0.0, v));
		}
	}

	VoxelHerd::VoxelHerd(const VoxelWorld& world, const HerdRules& rules, uint32_t seed)
		: world(&world), rules(rules), rng(seed) {
		shape.halfWidth = rules.halfWidth;
		shape.height = rules.creatureHeight;
	}

	VoxelHerd::Creature* VoxelHerd::find(const std::string& name) {
		auto found = indexByName.find(name);
		return found == indexByName.end() ? nullptr : &creatures[found->second];
	}

	const VoxelHerd::Creature* VoxelHerd::find(const std::string& name) const {
		auto found = indexByName.find(name);
		return found == indexByName.end() ? nullptr : &creatures[found->second];
	}

	void VoxelHerd::erase(size_t index) {
		creatures.erase(creatures.begin() + static_cast<std::ptrdiff_t>(index));
		indexByName.clear();
		for (size_t i = 0; i < creatures.size(); ++i) indexByName[creatures[i].name] = i;
	}

	double VoxelHerd::uniform(double low, double high) {
		return std::uniform_real_distribution<double>(low, high)(rng);
	}

	double VoxelHerd::choice(double a, double b) {
		return std::bernoulli_distribution(0.5)(rng) ? a : b;
	}

	void VoxelHerd::add(const std::string& name, const std::array<double, 3>& spawn, const HerdKind& kind) {
		if (find(name)) erase(indexByName[name]);
		Creature c;
		c.name = name;
		c.kind = kind;
		c.spawn = spawn;
		c.state.pos = spawn;
		c.heading = uniform(0.0, 2.0 * PI);
		c.timer = uniform(1.0, 4.0);
		c.routeTimer = uniform(0.0, rules.routeSeconds);
		c.health = kind.health;
		creatures.push_back(c);
		indexByName[name] = creatures.size() - 1;
		placeMesh(creatures.back());
	}

	void VoxelHerd::setPosition(Creature& c, double x, double y, double z) {
		c.state.pos = { x, y, z };
		c.state.vel = { 0.0, 0.0, 0.0 };
		c.state.onGround = false;
	}

	void VoxelHerd::setVelocity(Creature& c, double vx, double vz) {
		c.state.vel[0] = vx;
		c.state.vel[2] = vz;
	}

	// The body follows the creature: its feet, and its yaw (the model faces +Z, so the yaw is atan2(x, z); see VoxelWalkers.animate).
	void VoxelHerd::placeMesh(const Creature& c) {
		Body* body = findBodyByName(c.name);
		if (!body) return;
		Mesh* mesh = body->get<Mesh*>();
		if (!mesh) return;
		mesh->setPosition(Vector3f{ static_cast<float>(c.state.pos[0]), static_cast<float>(c.state.pos[1]), static_cast<float>(c.state.pos[2]) });
		const double yaw = std::atan2(std::cos(c.heading), std::sin(c.heading)) * 180.0 / PI;
		mesh->setRotation(Vector3f{ 0.0f, static_cast<float>(yaw), 0.0f });
	}

	void VoxelHerd::emitClip(Creature& c, const std::string& clip, bool looping) {
		events.push_back(HerdEvent{ HerdEvent::Kind::Clip, c.name, clip, looping });
		c.clip = clip;
	}

	double VoxelHerd::chaseDistance(const Creature& c, double night) const {
		const double base = rules.chaseRange * (1.0 + rules.nightChaseShare * clamp01(night));
		return c.kind.sight < 0.0 ? base : base * c.kind.sight / rules.chaseRange;
	}

	bool VoxelHerd::faces(const std::array<double, 3>& pos, const std::array<double, 2>& player, const std::array<double, 2>& shield) const {
		const double dx = pos[0] - player[0];
		const double dz = pos[2] - player[1];
		const double distance = std::hypot(dx, dz);
		if (distance < 1e-6) return true;
		return (dx * shield[0] + dz * shield[1]) / distance > rules.shieldCos;
	}

	void VoxelHerd::turnAway(Creature& c, double x, double z) {
		const auto& pos = c.state.pos;
		c.heading = std::atan2(pos[2] - z, pos[0] - x) + uniform(-0.3, 0.3);
		c.timer = uniform(1.0, 2.0);
	}

	bool VoxelHerd::isLava(int block) const {
		return std::find(rules.lava.begin(), rules.lava.end(), block) != rules.lava.end();
	}

	bool VoxelHerd::lavaAhead(const Creature& c, const std::array<double, 3>& pos) const {
		const double x = pos[0] + std::cos(c.heading) * rules.lookAhead;
		const double z = pos[2] + std::sin(c.heading) * rules.lookAhead;
		return isLava(world->getBlock(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(pos[1] + 0.1)), static_cast<int>(std::floor(z))));
	}

	bool VoxelHerd::planRoute(Creature& c, const std::array<double, 3>& pos, const std::array<double, 2>& player, const double* feet) {
		if (c.routeTimer > 0.0) return !c.route.empty();
		if (searchesLeft <= 0) return !c.route.empty();   // the frame's searches are used up: this creature tries again next frame
		--searchesLeft;
		c.routeTimer = rules.routeSeconds;
		const std::array<int, 3> here = { static_cast<int>(std::floor(pos[0])), static_cast<int>(std::floor(pos[1] + 0.1)),
			static_cast<int>(std::floor(pos[2])) };
		const int feetCell = feet ? static_cast<int>(std::floor(*feet + 0.1)) : here[1];
		const std::array<int, 3> goal = { static_cast<int>(std::floor(player[0])), feetCell, static_cast<int>(std::floor(player[1])) };
		std::vector<std::array<int, 3>> found;
		if (!world->findRoute(here, goal, rules.liquids, found)) found.clear();
		c.route = found;
		return !c.route.empty();
	}

	bool VoxelHerd::followRoute(Creature& c, const std::array<double, 3>& pos, double& heading) {
		while (!c.route.empty()) {
			const auto& cell = c.route.front();
			const double x = cell[0];
			const double z = cell[2];
			if (std::hypot(x + 0.5 - pos[0], z + 0.5 - pos[2]) >= rules.routeReach) {
				heading = std::atan2(z + 0.5 - pos[2], x + 0.5 - pos[0]);
				return true;
			}
			c.route.erase(c.route.begin());
		}
		return false;
	}

	void VoxelHerd::flash(Creature& c, double dt) {
		if (c.flashTimer <= 0.0) return;
		c.flashTimer = std::max(0.0, c.flashTimer - dt);
		if (c.flashTimer <= 0.0) events.push_back(HerdEvent{ HerdEvent::Kind::Material, c.name, c.kind.material, false });
	}

	void VoxelHerd::animate(Creature& c, double dt, double pace, bool bit) {
		if (bit) c.attackTimer = rules.attackSeconds;
		c.attackTimer = std::max(0.0, c.attackTimer - dt);
		// A flinch comes first, then a bite, then the gait. A role the kind has no clip for falls back to its walk.
		int role = Walk;
		if (c.hurtTimer > 0.0) role = Hurt;
		else if (c.attackTimer > 0.0) role = Attack;
		else if (pace > c.kind.pace + 0.1) role = Bound;
		std::string clip = c.kind.clips[role];
		if (clip.empty()) clip = c.kind.clips[Walk];
		const std::string& attack = c.kind.clips[Attack];
		const bool once = !clip.empty() && (clip == attack || clip == c.kind.clips[Hurt]);
		if (!clip.empty() && (clip != c.clip || (clip == attack && bit))) emitClip(c, clip, !once);
	}

	void VoxelHerd::bite(Creature& c) {
		c.attackTimer = rules.attackSeconds;
		const std::string& clip = c.kind.clips[Attack];
		if (!clip.empty() && clip != c.clip) emitClip(c, clip, false);
	}

	bool VoxelHerd::strike(Creature& c, const std::array<double, 2>& from, double damage, bool provoked) {
		if (c.removed) return false;
		c.health -= damage;
		if (provoked && !c.kind.bites && c.health > 0.0 && c.kind.fightBackSeconds >= 0.0) c.fightTimer = c.kind.fightBackSeconds;
		c.windup = 0.0;
		c.flashTimer = rules.flashSeconds;
		events.push_back(HerdEvent{ HerdEvent::Kind::Material, c.name, c.kind.flashMaterial, false });
		const std::string& hurtClip = c.kind.clips[Hurt];
		if (!hurtClip.empty()) {
			c.hurtTimer = rules.hurtSeconds;
			emitClip(c, hurtClip, false);
		}
		const auto& pos = c.state.pos;
		c.knockHeading = std::atan2(pos[2] - from[1], pos[0] - from[0]);
		c.knockTimer = rules.knockSeconds;
		return c.health <= 0.0;
	}

	void VoxelHerd::die(const std::string& name) {
		Creature* c = find(name);
		if (!c || c->removed) return;
		c->removed = true;
		const std::string& dieClip = c->kind.clips[Die];
		// A kind with no die clip is taken out at once; otherwise its body lies for the clip, then goes.
		c->dieTimer = dieClip.empty() ? 0.0 : rules.dieSeconds;
		if (!dieClip.empty()) emitClip(*c, dieClip, false);
	}

	void VoxelHerd::kill(Creature& c, Result& result) {
		die(c.name);
		result.killed.push_back(c.name);
	}

	bool VoxelHerd::alive(const std::string& name) const {
		const Creature* c = find(name);
		return c && !c->removed;
	}

	double VoxelHerd::health(const std::string& name) const {
		const Creature* c = find(name);
		return c ? c->health : 0.0;
	}

	VoxelBodyState VoxelHerd::state(const std::string& name) const {
		const Creature* c = find(name);
		return c ? c->state : VoxelBodyState{};
	}

	double VoxelHerd::heading(const std::string& name) const {
		const Creature* c = find(name);
		return c ? c->heading : 0.0;
	}

	void VoxelHerd::setHeading(const std::string& name, double heading, double timer) {
		Creature* c = find(name);
		if (!c) return;
		c->heading = heading;
		c->timer = timer;
	}

	void VoxelHerd::setPosition(const std::string& name, double x, double y, double z) {
		Creature* c = find(name);
		if (!c) return;
		setPosition(*c, x, y, z);
		placeMesh(*c);
	}

	bool VoxelHerd::hurt(const std::string& name, const std::array<double, 2>& from, double damage, bool provoked) {
		Creature* c = find(name);
		return c ? strike(*c, from, damage, provoked) : false;
	}

	double VoxelHerd::walk(Creature& c, double dt, const std::array<double, 2>* player, const double* playerFeet, double night,
		const std::array<double, 2>* shield, const std::array<double, 2>* prey) {
		flash(c, dt);
		c.hurtTimer = std::max(0.0, c.hurtTimer - dt);
		c.fightTimer = std::max(0.0, c.fightTimer - dt);
		const bool fighting = c.fightTimer > 0.0;
		std::array<double, 3> pos = c.state.pos;
		const bool onGround = c.state.onGround;
		if (pos[1] < rules.respawnDepth) {
			// A creature that falls out of the world goes back to its spawn point.
			setPosition(c, c.spawn[0], c.spawn[1], c.spawn[2]);
			c.hasLast = false;
			return 0.0;
		}
		c.biteTimer = std::max(0.0, c.biteTimer - dt);
		c.routeTimer = std::max(0.0, c.routeTimer - dt);
		if (c.knockTimer > 0.0) {
			// A hit pushes the creature away: it goes straight back, and its own choices wait until the push ends.
			c.knockTimer -= dt;
			setVelocity(c, std::cos(c.knockHeading) * rules.knockPace, std::sin(c.knockHeading) * rules.knockPace);
			c.last = pos;
			c.hasLast = true;
			return 0.0;
		}
		double bite = 0.0;
		double pace = c.kind.pace;
		c.timer -= dt;
		const bool hasPlayer = player != nullptr;
		double distance = hasPlayer ? std::hypot(pos[0] - (*player)[0], pos[2] - (*player)[1]) : INFINITY;
		const bool level = hasPlayer && (!playerFeet || (pos[1] < *playerFeet + rules.playerHeight && *playerFeet < pos[1] + rules.creatureHeight));
		if (level && distance < rules.contact) {
			// A frame that ends with the creature inside contact is set back on the contact distance, beside the player, before it moves.
			double dx = pos[0] - (*player)[0];
			double dz = pos[2] - (*player)[1];
			if (distance <= 0.0) {
				dx = std::cos(c.heading);
				dz = std::sin(c.heading);
			}
			double gap = std::hypot(dx, dz);
			if (gap == 0.0) gap = 1.0;
			const double x = (*player)[0] + dx / gap * rules.contact;
			const double z = (*player)[1] + dz / gap * rules.contact;
			setPosition(c, x, pos[1], z);
			pos = { x, pos[1], z };
			distance = rules.contact;
		}
		const bool chasing = (c.kind.bites || fighting) && distance < chaseDistance(c, night);
		if (!chasing) {
			c.route.clear();
			c.windup = 0.0;
			c.flankTimer = 0.0;
		}
		bool biting = false;   // a bite starts this frame: its clip plays now, and its damage lands biteWindup seconds later
		if (chasing) {
			// A biter heads for the player, and bites each time it is close and the last bite has had time to recover.
			c.heading = std::atan2((*player)[1] - pos[2], (*player)[0] - pos[0]);
			c.timer = 1.0;
			pace = c.kind.chasePace;
			double heading = 0.0;
			if (!c.route.empty() && followRoute(c, pos, heading)) c.heading = heading;
			if (c.flankTimer > 0.0) {
				// A creature a shield turned circles the player: it heads round to one side, until it is beside the shield.
				c.flankTimer -= dt;
				if (!shield || !faces(pos, *player, *shield)) {
					c.flankTimer = 0.0;
				} else {
					c.heading = std::atan2((*player)[1] - pos[2], (*player)[0] - pos[0]) + c.flankSide * rules.flankTurn;
				}
			}
			if (c.windup > 0.0) {
				// The lunge: the damage lands at the clip's strike, and only if the player is still in reach then.
				c.windup = std::max(0.0, c.windup - dt);
				if (c.windup <= 0.0 && distance < c.kind.biteRange && level) {
					if (shield && faces(pos, *player, *shield)) {
						// A shield held up in front of the player takes the bite, and the creature turns away from it.
						c.blocked = true;
						turnAway(c, (*player)[0], (*player)[1]);
						// Pushed back from the shield, as a hit pushes a creature: the same knock, away from the player.
						c.knockHeading = std::atan2(pos[2] - (*player)[1], pos[0] - (*player)[0]);
						c.knockTimer = rules.knockSeconds;
						// And it does not walk back into the shield: it circles round to bite from the side.
						c.flankSide = choice(-1.0, 1.0) > 0.0 ? 1 : -1;
						c.flankTimer = rules.flankSeconds;
					} else {
						bite = c.kind.biteDamage;
					}
				}
			} else if (distance < c.kind.biteRange && c.biteTimer <= 0.0 && level && c.flankTimer <= 0.0) {
				c.windup = rules.biteWindup;
				c.biteTimer = c.kind.biteInterval;
				biting = true;
			}
		} else if (c.kind.bites && prey) {
			// With no player near, a biter hunts the nearest timid creature (update does the biting).
			c.heading = std::atan2((*prey)[1] - pos[2], (*prey)[0] - pos[0]);
			c.timer = 1.0;
			pace = c.kind.chasePace;
		} else if (!c.kind.bites && distance < c.kind.fleeRange) {
			// Startled: it runs straight away from the player, and keeps running until it is out of range.
			c.heading = std::atan2(pos[2] - (*player)[1], pos[0] - (*player)[0]);
			c.timer = 1.0;
			pace = c.kind.fleePace;
		} else if (c.timer <= 0.0) {
			c.heading += uniform(-1.2, 1.2);
			c.timer = uniform(1.0, 4.0);
		}
		// A wall stops the creature short of the distance it meant to walk (seen in last frame's move): it turns, and jumps a block
		// if it stands on the ground. A creature held at the player's side is not stuck on a wall: it stands there and bites.
		const bool touching = level && distance < rules.contact + rules.contactSlack;
		if (c.hasLast && !touching) {
			const double walked = std::hypot(pos[0] - c.last[0], pos[2] - c.last[2]);
			if (walked < 0.5 * pace * dt && onGround) {
				double heading = 0.0;
				bool routed = false;
				if (chasing && planRoute(c, pos, *player, playerFeet)) routed = followRoute(c, pos, heading);
				if (routed) {
					c.heading = heading;   // a way round the wall: no random turn
				} else {
					c.heading += choice(-1.0, 1.0) * uniform(PI / 3.0, 2.0 * PI / 3.0);
				}
				if (c.state.onGround) c.state.vel[1] = rules.jumpSpeed;
			}
		}
		c.last = pos;
		c.hasLast = true;
		if (lavaAhead(c, pos)) {
			// Lava is a wall to a creature: it turns to one side of it, and keeps its other choices.
			c.heading += choice(-1.0, 1.0) * PI / 2.0;
		}
		double vx = std::cos(c.heading) * pace;
		double vz = std::sin(c.heading) * pace;
		if (hasPlayer && level) {
			// The player is a body too: a creature does not walk into it (the same contact rule as the player's own moves).
			const double limit = rules.contact + rules.contactSlack;
			const double dx = (*player)[0] - pos[0];
			const double dz = (*player)[1] - pos[2];
			const double gap = std::hypot(dx, dz);
			if (gap < limit && gap != 0.0) {
				const double nx = dx / gap;
				const double nz = dz / gap;
				const double toward = vx * nx + vz * nz;
				if (toward > 0.0) {
					vx -= toward * nx;
					vz -= toward * nz;
				}
			}
		}
		setVelocity(c, vx, vz);
		animate(c, dt, pace, biting);
		return bite;
	}

	void VoxelHerd::burn(Creature& c, double dt, Result& result) {
		const auto& pos = c.state.pos;
		const int x = static_cast<int>(std::floor(pos[0]));
		const int z = static_cast<int>(std::floor(pos[2]));
		const bool inLava = isLava(world->getBlock(x, static_cast<int>(std::floor(pos[1] + 0.1)), z))
			|| isLava(world->getBlock(x, static_cast<int>(std::floor(pos[1] + 1.0)), z));
		if (!inLava) {
			c.lavaTimer = 0.0;
			return;
		}
		c.lavaTimer += dt;
		while (c.lavaTimer >= rules.lavaInterval && !c.removed) {
			// A creature in lava loses one health every lavaInterval seconds, as a hit would.
			c.lavaTimer -= rules.lavaInterval;
			if (strike(c, { static_cast<double>(x), static_cast<double>(z) }, 1.0, true)) kill(c, result);
		}
	}

	VoxelHerd::Result VoxelHerd::update(double dt, const std::array<double, 2>* player, const double* playerFeet, double night,
		const std::array<double, 2>* shield) {
		Result result;
		searchesLeft = rules.routeSearchesPerFrame;

		// Corpses play their die clip, then are taken out.
		for (size_t i = 0; i < creatures.size();) {
			Creature& c = creatures[i];
			if (c.removed) {
				c.dieTimer -= dt;
				if (c.dieTimer <= 0.0) {
					events.push_back(HerdEvent{ HerdEvent::Kind::Remove, c.name, "", false });
					erase(i);
					continue;
				}
			}
			++i;
		}

		// The feet at the start of the frame: a creature's bump, prey and bite are judged on these, as they were when the frame began.
		std::vector<std::array<double, 3>> spots(creatures.size());
		std::vector<bool> live(creatures.size());
		for (size_t i = 0; i < creatures.size(); ++i) {
			spots[i] = creatures[i].state.pos;
			live[i] = !creatures[i].removed;
		}

		const double physicsDt = std::min(dt, MaxPhysicsStep);
		for (size_t i = 0; i < creatures.size(); ++i) {
			if (!live[i] || creatures[i].removed) continue;
			Creature& c = creatures[i];

			// A biter's prey: the nearest timid creature in its prey range, else the nearest wounded biter (a healthy one is not hunted).
			int prey = -1;
			if (c.kind.bites) {
				int nearestTimid = -1;
				int nearestWounded = -1;
				double timid = INFINITY;
				double wounded = INFINITY;
				for (size_t j = 0; j < creatures.size(); ++j) {
					const Creature& other = creatures[j];
					if (j == i || other.removed) continue;
					if (other.kind.bites && other.health >= other.kind.health) continue;
					const double distance = std::hypot(spots[i][0] - spots[j][0], spots[i][2] - spots[j][2]);
					if (distance >= c.kind.preyRange) continue;
					if (other.kind.bites) {
						if (distance < wounded) {
							wounded = distance;
							nearestWounded = static_cast<int>(j);
						}
					} else if (distance < timid) {
						timid = distance;
						nearestTimid = static_cast<int>(j);
					}
				}
				prey = nearestTimid >= 0 ? nearestTimid : nearestWounded;
			}
			std::array<double, 2> preyXz{};
			if (prey >= 0) preyXz = { spots[prey][0], spots[prey][2] };

			c.blocked = false;
			result.bites += walk(c, dt, player, playerFeet, night, shield, prey >= 0 ? &preyXz : nullptr);
			if (c.blocked) ++result.blocks;

			// Creatures about to bump turn away from each other, so the herd does not stack up in one place.
			for (size_t j = 0; j < spots.size(); ++j) {
				if (i != j && live[j] && std::hypot(spots[i][0] - spots[j][0], spots[i][2] - spots[j][2]) < rules.bumpRange) {
					turnAway(c, spots[j][0], spots[j][2]);
					break;
				}
			}

			// A prey already killed this frame (by another biter) is not hit again.
			if (prey >= 0) {
				Creature& victim = creatures[prey];
				if (c.biteTimer <= 0.0 && !victim.removed && std::hypot(spots[i][0] - spots[prey][0], spots[i][2] - spots[prey][2]) < c.kind.biteRange) {
					c.biteTimer = c.kind.biteInterval;
					bite(c);
					if (strike(victim, { spots[i][0], spots[i][2] }, 1.0, false)) kill(victim, result);
				}
			}

			c.state = world->stepBody(c.state, shape, physicsDt, false);
			placeMesh(c);
		}

		for (size_t i = 0; i < creatures.size(); ++i) {
			if (!creatures[i].removed) burn(creatures[i], dt, result);
		}
		return result;
	}

	int VoxelHerd::liveCount() const {
		int live = 0;
		for (const Creature& c : creatures) {
			if (!c.removed) ++live;
		}
		return live;
	}

	std::optional<std::array<double, 3>> VoxelHerd::findSpawn(const std::array<double, 2>& player, double feet,
		const std::array<double, 2>* look, int attempts, double minDistance, double maxDistance) {
		for (int attempt = 0; attempt < attempts; ++attempt) {
			const double angle = uniform(0.0, 2.0 * PI);
			const double distance = uniform(minDistance, maxDistance);
			const double x = player[0] + std::cos(angle) * distance;
			const double z = player[1] + std::sin(angle) * distance;
			if (look) {
				// Out of view: a spot the player is looking toward is not picked, so creatures do not pop in front of it.
				const double dx = x - player[0];
				const double dz = z - player[1];
				const double d = std::hypot(dx, dz);
				if (d > 0.0 && (dx * (*look)[0] + dz * (*look)[1]) / d > rules.lookCos) continue;
			}
			const int bx = static_cast<int>(std::floor(x));
			const int bz = static_cast<int>(std::floor(z));
			const int cx = static_cast<int>(std::floor(static_cast<double>(bx) / rules.chunk));
			const int cz = static_cast<int>(std::floor(static_cast<double>(bz) / rules.chunk));
			if (!world->isLoaded(cx, cz)) continue;
			// The ground is the top solid block at or below the feet (a little above them), above the water.
			const int top = std::min(rules.worldHeight - 2, static_cast<int>(std::floor(feet + 16.0)));
			int ground = -1;
			for (int y = top; y > rules.seaLevel; --y) {
				if (world->isSolid(bx, y, bz)) {
					ground = y;
					break;
				}
			}
			if (ground < 0 || ground + 1 <= rules.seaLevel) continue;
			// The creature's box needs room above the ground: no leaves or walls within a block of it.
			bool clear = true;
			for (int dx = -1; dx <= 1 && clear; ++dx) {
				for (int dz = -1; dz <= 1 && clear; ++dz) {
					for (int k = 0; k <= 1 && clear; ++k) {
						if (world->isSolid(bx + dx, ground + 1 + k, bz + dz)) clear = false;
					}
				}
			}
			if (!clear) continue;
			const double spotX = bx + 0.5;
			const double spotZ = bz + 0.5;
			const double spotY = ground + 1.0;
			bool crowded = false;
			for (const Creature& c : creatures) {
				if (c.removed) continue;
				if (std::hypot(c.state.pos[0] - spotX, c.state.pos[2] - spotZ) < rules.spacing) {
					crowded = true;
					break;
				}
			}
			if (crowded) continue;
			return std::array<double, 3>{ spotX, spotY, spotZ };
		}
		return std::nullopt;
	}

	bool VoxelHerd::holdsPlayer(const Creature& c, const std::array<double, 2>& player, double feet, const std::vector<std::string>& keep) const {
		for (const std::string& name : keep) {
			if (name == c.name) return true;
		}
		// A creature in a fight, a flinch, a lunge, a knock or a shield's turn is still dealing with the player.
		if (c.hurtTimer > 0.0 || c.fightTimer > 0.0 || c.flashTimer > 0.0 || c.windup > 0.0 || c.flankTimer > 0.0 || c.knockTimer > 0.0
			|| c.attackTimer > 0.0) return true;
		const auto& pos = c.state.pos;
		const double distance = std::hypot(pos[0] - player[0], pos[2] - player[1]);
		const bool level = pos[1] < feet + rules.playerHeight && feet < pos[1] + rules.creatureHeight;
		if (level && distance < rules.interactRange) return true;
		// A biter that could chase the player (at its longest, at night) is not taken out.
		return c.kind.bites && distance < chaseDistance(c, 1.0);
	}

	std::vector<std::string> VoxelHerd::despawn(const std::array<double, 2>& player, double feet, const std::vector<std::string>& keep,
		bool arrowsFlying, int target) {
		std::vector<std::string> gone;
		// While an arrow flies, it may still reach a creature the player shot at: none is taken out.
		if (arrowsFlying) return gone;
		std::vector<size_t> marked;
		auto isMarked = [&marked](size_t i) { return std::find(marked.begin(), marked.end(), i) != marked.end(); };
		int live = 0;
		for (size_t i = 0; i < creatures.size(); ++i) {
			const Creature& c = creatures[i];
			if (c.removed) continue;
			++live;
			if (holdsPlayer(c, player, feet, keep)) continue;
			if (std::hypot(c.state.pos[0] - player[0], c.state.pos[2] - player[1]) > rules.despawnRange) marked.push_back(i);
		}
		// Over the target, the farthest free creature beyond the spawn ring goes as well (one a call).
		if (live - static_cast<int>(marked.size()) > target) {
			double farthest = rules.spawnMin;
			size_t pick = creatures.size();
			for (size_t i = 0; i < creatures.size(); ++i) {
				const Creature& c = creatures[i];
				if (c.removed || isMarked(i) || holdsPlayer(c, player, feet, keep)) continue;
				const double distance = std::hypot(c.state.pos[0] - player[0], c.state.pos[2] - player[1]);
				if (distance > farthest) {
					farthest = distance;
					pick = i;
				}
			}
			if (pick < creatures.size()) marked.push_back(pick);
		}
		// Taken out by name, last first, so the indices still stand as they are removed.
		std::sort(marked.begin(), marked.end(), std::greater<size_t>());
		for (size_t i : marked) {
			gone.push_back(creatures[i].name);
			events.push_back(HerdEvent{ HerdEvent::Kind::Remove, creatures[i].name, "", false });
			erase(i);
		}
		return gone;
	}
}
