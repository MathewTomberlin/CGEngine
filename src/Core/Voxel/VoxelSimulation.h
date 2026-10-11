#pragma once

#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>

namespace CGEngine {
	class VoxelWorld;

	// Block physics for a VoxelWorld: falling blocks, flowing water and reaction rules, run on the cells that changed.
	//
	// A cell is queued when it or a neighbour changes (a block set, a block that moved). Each tick takes the queued cells
	// and looks at each one: a falling block moves down, water falls and spreads, and reaction rules and the optional
	// Python hook may replace the block. A change queues the cells around it, so a change spreads only as far as it
	// causes one. Unloaded chunks are not simulated, and a quiet world costs nothing: that is what keeps far, idle chunks
	// free. Ticks run every tickSeconds; a budget bounds the work in one call.
	class VoxelSimulation {
	public:
		// A reaction: a block that has a neighbour of the given kind becomes another block, with a chance per tick.
		struct Rule {
			uint8_t block;
			uint8_t neighbour;
			uint8_t becomes;
			float chance;
		};

		// The hook sees a queued block once per tick and returns the block it becomes, or -1 to leave it.
		using Hook = std::function<int(int x, int y, int z, int block)>;

		void activate(int x, int y, int z);
		void addRule(const Rule& rule) { rules.push_back(rule); }
		void clearRules() { rules.clear(); }
		void setHook(Hook newHook) { hook = std::move(newHook); }
		// Accumulates dt and runs the ticks that are due, each within budgetMs of the call. Returns the cells looked at.
		int simulate(VoxelWorld& world, double dt, double budgetMs, double tickSeconds = 0.1);
		int activeCount() const { return static_cast<int>(queue.size()); }

	private:
		std::vector<int64_t> queue;
		std::unordered_set<int64_t> queued;
		std::vector<Rule> rules;
		Hook hook;
		double accumulator = 0.0;
		uint32_t randomState = 0x9E3779B9u;

		static int64_t keyOf(int x, int y, int z);
		static void unkey(int64_t key, int& x, int& y, int& z);
		float nextRandom();
		// Looks at one cell: returns the cells it changed or queued through the world, and whether it changed anything.
		void tickCell(VoxelWorld& world, int x, int y, int z);
		void set(VoxelWorld& world, int x, int y, int z, uint8_t block);
	};
}
