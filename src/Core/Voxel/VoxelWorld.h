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
		// The quads in a loaded chunk's mesh as last uploaded (0 if it has none), all groups together.
		int chunkQuads(int cx, int cz) const;
		// The quads of each mesh group a loaded chunk has, by group: 0 is the terrain, else the block id with its own mesh.
		std::map<int, int> chunkGroupQuads(int cx, int cz) const;
		// The generator's settings. Chunks requested later use them; loaded chunks keep what they have.
		void setTerrain(const VoxelTerrain& settings) { terrain = settings; }
		/// The biome of the column at world (x, z): "plains", "forest" or "desert" (see VoxelTerrain::biomeAt).
		const char* biomeName(int x, int z) const;

		int getBlock(int x, int y, int z) const;
		// Sets a block, and queues it and its neighbours for the simulation. Unloaded chunks keep the edit for later.
		void setBlock(int x, int y, int z, int blockId);
		// A torch: a prop in the air cell (x, y, z), leaning on the block in direction dir (VoxelMesher.h PropDirs, 0 to 5). It
		// gives light and is drawn, but it is not a block. False if the cell is a block or its chunk is not loaded.
		bool setProp(int x, int y, int z, int dir);
		void removeProp(int x, int y, int z);
		// The dir of the prop at (x, y, z), or -1 for none.
		int propAt(int x, int y, int z) const;
		// How far from the cell's centre, toward the block the prop at (x, y, z) leans on (dir), the drawn skin is: a torch stands
		// on that surface (VoxelMesher.h propShape). 0.5 if there is no solid block there.
		float propAnchor(int x, int y, int z, int dir) const;
		// Hits: a block takes BlockType::hits hits to break. Each hit adds one, and the damage shows in the block's colour. Returns the
		// damage stage the block is at now (1 to hits - 1), -1 when this hit breaks it (it becomes air), or -2 if there is no block.
		int hitBlock(int x, int y, int z);
		// The damage stage of the block at (x, y, z), 0 for none.
		int damageAt(int x, int y, int z) const;
		// Every prop as (x, y, z, dir). Props are kept with the edits, so a chunk that unloads keeps them.
		std::vector<std::array<int, 4>> props() const;
		// True if the column at (x, z) is in a loaded chunk, so its blocks are real.
		bool loadedAt(int x, int z) const;

		// What the blocks do (see VoxelBlocks.h): solid for collision, falling, or liquid, and how they are drawn (own, cube).
		// Set it for any id, once. A cube block is always drawn in its own mesh, so cube implies own.
		void defineBlock(int blockId, const BlockType& type);
		const BlockType& blockType(int blockId) const { return types[static_cast<size_t>(std::clamp(blockId, 0, 255))]; }
		bool isSolid(int x, int y, int z) const;
		// The material a block's own mesh is drawn with (BlockType::own). Blocks without one use the world's material.
		void setMaterial(int blockId, const std::string& name);

		// The skins that collision sees (see skinDensity): 0 is air, TerrainSkin the terrain, OwnSkin + id a smooth block's own
		// skin and CubeSkin + id a cube block's. A solid block is in exactly one.
		static constexpr int TerrainSkin = 1;
		static constexpr int OwnSkin = 256;
		static constexpr int CubeSkin = 512;
		int skinAt(int x, int y, int z) const;

		/// The sky light at a block, 0 (none) to 255 (full), as the mesh shades it. Computed on demand, so this is for queries.
		uint8_t lightAt(int x, int y, int z) const;
		// Moves a box of this shape through the blocks (see VoxelBody.h). The C++ twin of VoxelPhysics.step.
		VoxelBodyState stepBody(const VoxelBodyState& body, const VoxelBodyShape& shape, double dt, bool flying) const;

		/// The skin's density at world position (x, y, z). Each smooth skin (the terrain, and each block type with its own mesh)
		/// is the occupancy of its blocks' centres around the point, weighted trilinearly; the density is the largest of them.
		/// A cube block's skin is 1 in its own cell and 0 outside. A point is inside the drawn surface where this is above
		/// VoxelSkinIso. Solid blocks occupy it (water and air do not), and cells of chunks that are not loaded count as solid
		/// terrain, so a body cannot walk out of the loaded world.
		double skinDensity(double x, double y, double z) const;

		/// The first point along the ray from origin (direction is a unit vector) where the skin begins, within maxDistance. False
		/// if the origin is already inside the skin or nothing is hit. It is the surface the player sees under the crosshair.
		bool skinSurface(const std::array<double, 3>& origin, const std::array<double, 3>& direction, double maxDistance,
			std::array<double, 3>& surface) const;
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
			int quads = 0;                  // all groups together
			int resolution = 0;             // the smooth skin's samples per block for this chunk (0: cubes); see resolutionFor
			std::map<int, id_t> bodies;     // the body of each mesh group drawn (0: the terrain), by its asset id
			std::map<int, int> groupQuads;  // the quads of each group's mesh as last uploaded
		};

		struct TerrainResult {
			int cx;
			int cz;
			uint32_t generation;
			std::vector<uint8_t> blocks;
		};

		// One group's mesh of a chunk (see buildChunkMesh): group 0 is the terrain.
		struct GroupMesh {
			uint8_t group = 0;
			MeshArrays mesh;
			int quads = 0;
		};

		struct MeshResult {
			int cx;
			int cz;
			uint32_t generation;
			uint32_t version;
			std::vector<GroupMesh> groups;
		};

		int size;
		int height;
		std::string material;
		std::array<glm::vec3, 256> palette;
		std::array<BlockType, 256> types;
		// The material of each block's own mesh, by block id; empty for the world's material.
		std::array<std::string, 256> groupMaterials;
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
		// Props (torches) by chunk: local index -> dir. See setProp.
		std::map<std::pair<int, int>, std::map<int, uint8_t>> propsByChunk;
		// Damage by chunk: local index -> stage (see hitBlock). Not saved with the edits yet.
		std::map<std::pair<int, int>, std::map<int, uint8_t>> damageByChunk;
		void clearDamage(int x, int y, int z);
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
		// Marks the chunks that see the light of the cell at local (lx, lz) in chunk (cx, cz), as setBlock does.
		void markLight(int cx, int cz, int lx, int lz);
		void installTerrain(TerrainResult& result);
		void applyEdits(int cx, int cz, std::vector<uint8_t>& blocks) const;
		void uploadMesh(MeshResult& result);
		void startMeshing(int cx, int cz, Chunk& chunk);
		PaddedChunk snapshot(int cx, int cz, int border) const;
		std::string bodyName(int cx, int cz, uint8_t group) const;
	};
}
