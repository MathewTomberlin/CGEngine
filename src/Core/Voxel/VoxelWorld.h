#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <fstream>
#include "VoxelBlocks.h"
#include "VoxelBody.h"
#include "VoxelMesher.h"
#include "VoxelSimulation.h"
#include "VoxelTerrain.h"

namespace CGEngine {
	// A world of blocks split into square chunks, built in the background. A chunk goes through three steps:
	//   requestChunk: a worker generates its blocks (VoxelTerrain);
	//   update: the blocks are installed, and the chunk is marked dirty;
	//   update: a worker meshes the dirty chunk from a copy of it and its neighbours (VoxelMesher), and update uploads
	//           the mesh as a body (see RuntimeMesh.h) within a time budget.
	// Only update() and the block setters touch the world; workers see copies. Block 0 is air. Blocks in chunks that are
	// not loaded count as air. Chunk (cx, cz) covers x in [cx * size, cx * size + size) and z likewise, and its body sits
	// at that corner, so its vertices are local to the chunk.
	class VoxelWorld {
	public:
		VoxelWorld(int chunkSize, int chunkHeight, const std::string& materialName);
		~VoxelWorld();
		VoxelWorld(const VoxelWorld&) = delete;
		VoxelWorld& operator=(const VoxelWorld&) = delete;

		int chunkSize() const { return size; }
		int chunkHeight() const { return height; }

		void setColour(int blockId, glm::vec3 colour);
		// The surface style: 0 draws the blocks as cubes (greedy-merged faces); n >= 1 draws a smooth skin over them with n
		// samples per block (see buildSmoothMesh). Changing it re-meshes every loaded chunk.
		void setSmoothing(int resolution);
		int smoothing() const { return smoothResolution; }
		// Level of detail for the smooth skin: the chunks within nearChunks of the focus (see update) use nearSamples per
		// block, the rest use the smoothing resolution. nearChunks 0 turns it off.
		void setDetail(int nearChunks, int nearSamples);
		int resolutionFor(int cx, int cz) const;
		// The quads in a loaded chunk's mesh as last uploaded (0 if it has none).
		int chunkQuads(int cx, int cz) const;
		// The generator's settings. Chunks requested later use them; loaded chunks keep what they have.
		void setTerrain(const VoxelTerrain& settings) { terrain = settings; }

		int getBlock(int x, int y, int z) const;
		// Sets a block, and queues it and its neighbours for the simulation. Unloaded chunks keep the edit for later.
		void setBlock(int x, int y, int z, int blockId);
		// True if the column at (x, z) is in a loaded chunk, so its blocks are real.
		bool loadedAt(int x, int z) const;

		// What the blocks do (see VoxelBlocks.h): solid for collision, falling, or liquid. Set it for any id, once.
		void defineBlock(int blockId, const BlockType& type);
		const BlockType& blockType(int blockId) const { return types[static_cast<size_t>(std::clamp(blockId, 0, 255))]; }
		bool isSolid(int x, int y, int z) const;

		/// The sky light at a block, 0 (none) to 255 (full), as the mesh shades it. Computed on demand, so this is for queries.
		uint8_t lightAt(int x, int y, int z) const;
		// Moves a box of this shape through the blocks (see VoxelBody.h). The C++ twin of VoxelPhysics.step.
		VoxelBodyState stepBody(const VoxelBodyState& body, const VoxelBodyShape& shape, double dt, bool flying) const;
		// Runs the simulation for dt seconds, within budgetMs of work (see VoxelSimulation). Returns the cells looked at.
		int simulate(double dt, double budgetMs, double tickSeconds = 0.1) {
			return simulation.simulate(*this, dt, budgetMs, tickSeconds);
		}
		// Falling blocks take this long to move one cell (0 for instant moves). They are drawn by the game while in flight.
		void setMoveSeconds(double seconds) { simulation.setMoveSeconds(seconds); }
		const std::vector<VoxelSimulation::Move>& movingBlocks() const { return simulation.inFlight(); }
		// Lands every block in flight now, so the world is complete (before it is saved, for example).
		void settleMoves() { simulation.settleMoves(*this); }
		VoxelSimulation& physics() { return simulation; }
		// Sets the blocks from yFrom (inclusive) to yTo (exclusive), clipped to the world height.
		void fillColumn(int x, int z, int yFrom, int yTo, int blockId);

		// Starts generating chunk (cx, cz) on a worker. False if it is requested or loaded already.
		bool requestChunk(int cx, int cz);
		// True once the chunk's blocks are installed.
		bool isLoaded(int cx, int cz) const;
		// Removes a chunk and its body (if it has one). Its neighbours are re-meshed, since their faces against it now show.
		void unloadChunk(int cx, int cz);
		// Installs finished terrain, uploads finished meshes until budgetMs have passed (at least one, if there is one),
		// and starts meshing up to a few dirty chunks, nearest to the focus chunk first. Returns the meshes uploaded.
		int update(double budgetMs, int focusX, int focusZ);

		int chunkCount() const { return static_cast<int>(chunks.size()); }
		int loadedCount() const;
		int pendingCount() const;       // chunks waiting for terrain
		int dirtyCount() const;         // loaded chunks whose mesh is out of date
		int meshingCount() const;       // chunks being meshed on a worker
		int quadCount() const;          // quads in the loaded chunks' meshes, as last uploaded
		double lastUploadMs() const { return uploadMs; }
		// Removes every chunk and its body. Work still on the workers is dropped when it finishes. The edits stay.
		void clear();

		// Block edits are kept apart from the chunks, so they survive unloading and are applied again when a chunk loads.
		// The file is binary: "CGVE", a version (1), a count, then per edit the int32 x, y, z and the uint8 block id.
		bool saveEdits(const std::string& path) const;
		// Adds the file's edits to the world's (a block edited in both takes the file's value). False if the file is unreadable.
		bool loadEdits(const std::string& path);
		int editCount() const;
		void clearEdits() { edits.clear(); }

	private:
		struct Chunk {
			std::vector<uint8_t> blocks;    // empty until the terrain is installed
			bool loaded = false;
			uint32_t generation = 0;        // unique per chunk, so late results for an unloaded chunk are ignored
			bool dirty = true;              // the mesh is out of date
			bool meshing = false;           // a mesh for the current version is on a worker
			uint32_t version = 0;           // bumped by each change to the blocks or their neighbours
			int quads = 0;
			int resolution = 0;             // the smooth skin's samples per block for this chunk (0: cubes); see resolutionFor
			std::pair<bool, id_t> body = { false, 0 };   // the body's asset id, once uploaded
		};

		struct TerrainResult {
			int cx;
			int cz;
			uint32_t generation;
			std::vector<uint8_t> blocks;
		};

		struct MeshResult {
			int cx;
			int cz;
			uint32_t generation;
			uint32_t version;
			MeshArrays mesh;
			int quads;
		};

		int size;
		int height;
		std::string material;
		std::array<glm::vec3, 256> palette;
		std::array<BlockType, 256> types;
		VoxelSimulation simulation;
		VoxelTerrain terrain;
		int smoothResolution = 0;
		int nearRadius = 0;
		int nearResolution = 2;
		int focusCx = 0;
		int focusCz = 0;
		bool focusKnown = false;
		// The blocks in flight, drawn as one mesh of balls (see updateMovingBlocks).
		std::pair<bool, id_t> movingBody = { false, 0 };
		bool movingShown = false;
		std::map<std::pair<int, int>, Chunk> chunks;
		// The chunks whose mesh may be out of date, so update() finds them without scanning every chunk.
		std::set<std::pair<int, int>> dirtyKeys;
		// Edited blocks: chunk (cx, cz) -> index in that chunk -> block id. See setBlock.
		std::map<std::pair<int, int>, std::map<int, uint8_t>> edits;
		uint32_t nextGeneration = 1;
		double uploadMs = 0.0;

		// Background work. Jobs run on the workers; their results wait in the result queues until update() takes them.
		std::vector<std::thread> workers;
		std::mutex jobMutex;
		std::condition_variable jobReady;
		std::deque<std::function<void()>> jobs;
		bool stopping = false;
		std::mutex resultMutex;
		std::deque<TerrainResult> terrainResults;
		std::deque<MeshResult> meshResults;

		void startWorkers();
		// Re-reads the detail of every loaded chunk for a new focus; a chunk whose level changed is meshed again.
		void refreshResolutions(int focusX, int focusZ, bool force);
		void updateMovingBlocks();
		void stopWorkers();
		void workerLoop();
		void enqueue(std::function<void()> job);

		static int floorDiv(int a, int b);
		int localIndex(int lx, int y, int lz) const { return (y * size + lz) * size + lx; }
		const Chunk* findChunk(int cx, int cz) const;
		Chunk* findChunk(int cx, int cz);
		void markDirty(int cx, int cz);
		void markNeighboursDirty(int cx, int cz);
		void installTerrain(TerrainResult& result);
		void applyEdits(int cx, int cz, std::vector<uint8_t>& blocks) const;
		void uploadMesh(MeshResult& result);
		void startMeshing(int cx, int cz, Chunk& chunk);
		PaddedChunk snapshot(int cx, int cz, int border) const;
		std::string bodyName(int cx, int cz) const;
	};
}
