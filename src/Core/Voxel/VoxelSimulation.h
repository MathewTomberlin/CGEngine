#pragma once

#include <bitset>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>
#include "VoxelBlocks.h"

namespace CGEngine {
	class VoxelWorld;

	// Block physics for a VoxelWorld: falling blocks, flowing water and reaction rules, run on the cells that changed.
	//
	// A cell is queued when it or a neighbour changes (a block set, a block that moved). Each tick takes the queued cells
	// and looks at each one: a falling block moves down, water falls and spreads, and reaction rules and the optional
	// Python hook may replace the block. A change queues the cells around it, so a change spreads only as far as it
	// causes one. Unloaded chunks are not simulated, and a quiet world costs nothing: that is what keeps far, idle chunks
	// free. Ticks run every tickSeconds; a budget bounds the work in one call.
	//
	// A falling block moves over moveSeconds rather than at once: its source is cleared when the move starts and its
	// destination is written when the move ends, and in between it is "in flight", drawn by the game (see inFlight).
	// With moveSeconds at 0, moves are instant.
	class VoxelSimulation {
	public:
		// A reaction: a block that touches one of the neighbour kinds becomes another block. With seconds at 0 the change comes with
		// the chance per tick. With seconds above 0 the block must keep touching for that long: it shows the progress as damage
		// stages (out of its hits) and then becomes the other block, as a block breaks when it is hit.
		struct Rule {
			uint8_t block;
			std::bitset<256> neighbours;
			uint8_t becomes;
			float chance;
			float seconds;
			bool diagonal = false;   // the neighbours are the 26 around the cell (edges and corners too), not the six faces
		};

		// A block on its way from one cell to the cell below (or any neighbour). Progress runs from 0 to 1.
		struct Move {
			int from[3];
			int to[3];
			uint8_t block;
			double elapsed;
			double duration;
		};

		// The hook sees a queued block once per tick and returns the block it becomes, or -1 to leave it.
		using Hook = std::function<int(int x, int y, int z, int block)>;

		void activate(int x, int y, int z);
		void addRule(const Rule& rule) { rules.push_back(rule); }
		void clearRules() { rules.clear(); }
		void setHook(Hook newHook) { hook = std::move(newHook); }
		// How long a falling block takes to move one cell (0 for an instant move).
		void setMoveSeconds(double seconds) { moveDuration = seconds; }
		double moveSeconds() const { return moveDuration; }
		// Accumulates dt, advances the blocks in flight, and runs the ticks that are due, each within budgetMs of the call.
		// Returns the cells looked at.
		int simulate(VoxelWorld& world, double dt, double budgetMs, double tickSeconds = 0.1);
		int activeCount() const { return static_cast<int>(queue.size()); }
		const std::vector<Move>& inFlight() const { return moves; }
		// Ends every move now: the blocks land where they were going.
		void settleMoves(VoxelWorld& world);

	private:
		std::vector<int64_t> queue;
		std::unordered_set<int64_t> queued;
		std::vector<Rule> rules;
		// A timed reaction under way, by cell: which rule it is, and how long it has run.
		struct Progress {
			uint32_t rule;
			double elapsed;
		};
		std::unordered_map<int64_t, Progress> progress;
		Hook hook;
		double accumulator = 0.0;
		uint32_t randomState = 0x9E3779B9u;
		double moveDuration = 0.0;
		std::vector<Move> moves;
		std::unordered_set<int64_t> reserved;   // destinations of moves in flight, which no other block may take

		static int64_t keyOf(int x, int y, int z);
		static void unkey(int64_t key, int& x, int& y, int& z);
		float nextRandom();
		// Looks at one cell and changes what is there, through the world.
		void tickCell(VoxelWorld& world, int x, int y, int z, double tickSeconds);
		// A volume's tick: it falls whole into air, or into the same volume below up to a full cell, then evens out with its
		// sides (a side with at least two units less takes one). Units are moved, never made or lost.
		void tickVolume(VoxelWorld& world, int x, int y, int z, const VolumeLiquid& volume);
		// Applies the reactions of a cell. Returns true when the cell became another block at once.
		bool react(VoxelWorld& world, int x, int y, int z, uint8_t block, double tickSeconds);
		void set(VoxelWorld& world, int x, int y, int z, uint8_t block);
		// Starts a move: the source is cleared now, and the block is written at the destination when the move ends.
		void beginMove(VoxelWorld& world, int fromX, int fromY, int fromZ, int toX, int toY, int toZ, uint8_t block);
		void advanceMoves(VoxelWorld& world, double dt);
	};
}
