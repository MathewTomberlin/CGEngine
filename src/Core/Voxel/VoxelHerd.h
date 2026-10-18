#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include "VoxelBody.h"

namespace CGEngine {
	class VoxelWorld;

	// The numbers the herd's rules use (resources/scripts/VoxelWalkers.py sends them once, from its constants).
	struct HerdRules {
		double respawnDepth = -10.0;
		double bumpRange = 0.8;
		double lookAhead = 1.0;
		double knockSeconds = 0.25;
		double knockPace = 4.5;
		double flashSeconds = 0.15;
		double hurtSeconds = 0.37;
		double dieSeconds = 0.97;
		double shieldCos = 0.5;
		double flankSeconds = 4.0;
		double flankTurn = 1.0471975511965976;   // 60 degrees
		double attackSeconds = 0.8;
		double biteWindup = 0.3;
		double routeSeconds = 1.0;
		int routeSearchesPerFrame = 2;
		double routeReach = 0.45;
		double contact = 0.0;             // physics.CONTACT
		double contactSlack = 0.0;        // physics.CONTACT_SLACK
		double jumpSpeed = 0.0;           // physics.JUMP_SPEED
		double playerHeight = 1.8;
		double halfWidth = 0.3;
		double creatureHeight = 1.2;
		double lavaInterval = 0.5;
		double chaseRange = 7.0;          // VoxelNight.CHASE_RANGE
		double nightChaseShare = 0.8;     // VoxelNight.NIGHT_CHASE_SHARE
		std::vector<int> lava;            // VoxelGen.LAVA_BLOCKS
		std::vector<int> liquids;         // VoxelGen.LIQUIDS
	};

	// A kind of creature (resources/voxel_creatures.json). Its clips are by role (walk, bound, attack, hurt, die); an empty name
	// is a role the kind has no clip for.
	struct HerdKind {
		bool bites = false;
		double health = 3.0;
		double pace = 1.4;
		double chasePace = 2.0;
		double fleePace = 3.0;
		double fleeRange = 0.0;
		double biteRange = 1.3;
		double biteDamage = 1.0;
		double biteInterval = 1.2;
		double preyRange = 0.0;
		double sight = -1.0;              // a biter's day sight in blocks; negative for the default chase range
		double fightBackSeconds = -1.0;   // a timid kind fights back this long after a hit; negative for none
		std::string material;
		std::string flashMaterial;
		std::array<std::string, 5> clips;   // walk, bound, attack, hurt, die
	};

	// Something the game must do to the engine's bodies, which the herd cannot do itself: a clip to play, a material to draw, a body
	// to take out. Drained with takeEvents.
	struct HerdEvent {
		enum class Kind { Clip, Material, Remove };
		Kind kind;
		std::string name;      // the body
		std::string value;     // the clip or material ("" for Remove)
		bool looping = false;  // for Clip
	};

	// A herd of creatures: each is a box that walks the terrain (VoxelBody physics) and decides where to go each frame: wander, flee the
	// player, chase and bite it, hunt a wounded biter, plan a route round a wall, and burn in lava. The game sends the player once a frame,
	// and the herd moves every creature, places its mesh (position and yaw) and reports what the game must do (HerdEvent) and what happened
	// (bites, blocks, kills). Creatures live until they are taken out; a corpse plays its die clip, then goes.
	class VoxelHerd {
	public:
		struct Result {
			double bites = 0.0;     // health the biters took from the player this frame
			int blocks = 0;         // shield blocks this frame
			std::vector<std::string> killed;   // creatures that died this frame (by a bite or by lava), once each
		};

		VoxelHerd(const VoxelWorld& world, const HerdRules& rules, uint32_t seed);

		// A creature at feet spawn, standing on the terrain. Its mesh body must already exist under name.
		void add(const std::string& name, const std::array<double, 3>& spawn, const HerdKind& kind);

		// Moves every creature for dt seconds. player is the player's (x, z), or null when there is none; playerFeet its height, or null
		// when the creatures are level with it; shield the (x, z) the player faces while holding a shield, or null.
		Result update(double dt, const std::array<double, 2>* player, const double* playerFeet, double night,
			const std::array<double, 2>* shield);

		// A hit from the point from (x, z): returns true when it kills the creature. The death itself is die().
		bool hurt(const std::string& name, const std::array<double, 2>& from, double damage, bool provoked);

		// The creature dies: it stops moving at once, and its body plays the die clip, then goes (a Remove event).
		void die(const std::string& name);

		// Whether the creature is alive (exists and is not dying).
		bool alive(const std::string& name) const;
		double health(const std::string& name) const;
		// The feet, velocity and ground contact of the creature (VoxelWalker's state).
		VoxelBodyState state(const std::string& name) const;
		int count() const { return static_cast<int>(creatures.size()); }
		// For the checks and the console: the creature's heading, a way to turn it and set its wander timer, and to place its feet.
		double heading(const std::string& name) const;
		void setHeading(const std::string& name, double heading, double timer);
		void setPosition(const std::string& name, double x, double y, double z);

		std::vector<HerdEvent> takeEvents() {
			std::vector<HerdEvent> out;
			out.swap(events);
			return out;
		}

	private:
		struct Creature {
			std::string name;
			HerdKind kind;
			std::array<double, 3> spawn{};
			VoxelBodyState state;
			double heading = 0.0;
			double timer = 0.0;
			double knockTimer = 0.0;
			double knockHeading = 0.0;
			double flankTimer = 0.0;
			int flankSide = 1;
			double biteTimer = 0.0;
			double fightTimer = 0.0;
			double windup = 0.0;
			double flashTimer = 0.0;
			double hurtTimer = 0.0;
			double dieTimer = 0.0;
			double attackTimer = 0.0;
			double routeTimer = 0.0;
			double lavaTimer = 0.0;
			double health = 0.0;
			bool hasLast = false;
			std::array<double, 3> last{};
			bool removed = false;     // dead: a corpse until its die clip has played
			bool blocked = false;     // a shield turned this frame's bite
			std::string clip;         // the clip it plays ("" for none)
			std::vector<std::array<int, 3>> route;
		};

		const VoxelWorld* world;
		HerdRules rules;
		VoxelBodyShape shape;
		std::mt19937 rng;
		std::vector<Creature> creatures;
		std::unordered_map<std::string, size_t> indexByName;
		std::vector<HerdEvent> events;
		int searchesLeft = 0;

		Creature* find(const std::string& name);
		const Creature* find(const std::string& name) const;
		void erase(size_t index);

		double uniform(double low, double high);
		double choice(double a, double b);   // one of the two, at random

		void setPosition(Creature& c, double x, double y, double z);
		void setVelocity(Creature& c, double vx, double vz);
		void placeMesh(const Creature& c);
		void emitClip(Creature& c, const std::string& clip, bool looping);

		double chaseDistance(const Creature& c, double night) const;
		bool faces(const std::array<double, 3>& pos, const std::array<double, 2>& player, const std::array<double, 2>& shield) const;
		void turnAway(Creature& c, double x, double z);
		bool lavaAhead(const Creature& c, const std::array<double, 3>& pos) const;
		bool planRoute(Creature& c, const std::array<double, 3>& pos, const std::array<double, 2>& player, const double* feet);
		bool followRoute(Creature& c, const std::array<double, 3>& pos, double& heading);
		void flash(Creature& c, double dt);
		void animate(Creature& c, double dt, double pace, bool bit);
		// The creature's own decisions for the frame (VoxelWalkers.Walker.update): returns the health a bite takes from the player.
		double walk(Creature& c, double dt, const std::array<double, 2>* player, const double* playerFeet, double night,
			const std::array<double, 2>* shield, const std::array<double, 2>* prey);
		// A hit on the creature (see hurt): true when it kills it.
		bool strike(Creature& c, const std::array<double, 2>& from, double damage, bool provoked);
		// A biter's bite on a creature (prey): its attack clip plays now.
		void bite(Creature& c);
		void burn(Creature& c, double dt, Result& result);
		void kill(Creature& c, Result& result);
		bool isLava(int block) const;
	};
}
