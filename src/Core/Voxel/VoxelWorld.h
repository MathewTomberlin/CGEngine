#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "VoxelMesher.h"
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
		// The generator's settings. Chunks requested later use them; loaded chunks keep what they have.
		void setTerrain(const VoxelTerrain& settings) { terrain = settings; }

		int getBlock(int x, int y, int z) const;
		void setBlock(int x, int y, int z, int blockId);
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
		// Removes every chunk and its body. Work still on the workers is dropped when it finishes.
		void clear();

	private:
		struct Chunk {
			std::vector<uint8_t> blocks;    // empty until the terrain is installed
			bool loaded = false;
			uint32_t generation = 0;        // unique per chunk, so late results for an unloaded chunk are ignored
			bool dirty = true;              // the mesh is out of date
			bool meshing = false;           // a mesh for the current version is on a worker
			uint32_t version = 0;           // bumped by each change to the blocks or their neighbours
			int quads = 0;
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
		VoxelTerrain terrain;
		std::map<std::pair<int, int>, Chunk> chunks;
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
		void uploadMesh(MeshResult& result);
		void startMeshing(int cx, int cz, Chunk& chunk);
		PaddedChunk snapshot(int cx, int cz) const;
		std::string bodyName(int cx, int cz) const;
	};
}
