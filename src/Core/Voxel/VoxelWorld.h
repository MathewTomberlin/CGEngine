#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
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
	// One section's meshes, one per group (see buildChunkMesh), in the section's own coordinates. A section keeps the one it was last
	// built to (see Section::mesh), so a change to one section meshes that section and not the rest of its region.
	struct ChunkMesh {
		std::map<int, MeshArrays> groups;
	};


	// A world of blocks split into square sections, built in the background. A section is a cube of blocks, size on a side, keyed by
	// its place (cx, cy, cz): block (x, y, z) is in section (floor(x / size), floor(y / size), floor(z / size)). A section goes through
	// three steps:
	//   requestChunk: a worker generates its blocks (VoxelTerrain);
	//   update: the blocks are installed, and the section is marked changed, with the neighbours its light and skin reach;
	//   update: a worker meshes the changed sections of a region from copies of them and their neighbours (VoxelMesher), and
	//           update uploads the region's mesh as one body per mesh group (see RuntimeMesh.h) within a time budget.
	// A region is RegionChunks by RegionChunks by RegionChunks sections (see VoxelWorld.cpp), so the view draws a region's groups in one
	// body each instead of one per section. A section keeps the mesh it was last built to, so a change re-meshes only the sections it touches.
	// Only update() and the block setters touch the world; workers see copies. Block 0 is air. Blocks in sections that are not loaded
	// count as air. Section (cx, cy, cz) covers x in [cx * size, cx * size + size) and likewise y and z, and its body sits at that
	// corner, so its vertices are local to the section.
	// The world is not bounded in height: a section that is not loaded counts as air, except where the terrain itself is read.
	class VoxelWorld {
	public:
		VoxelWorld(int chunkSize, int chunkHeight, const std::string& materialName);
		~VoxelWorld();
		VoxelWorld(const VoxelWorld&) = delete;
		VoxelWorld& operator=(const VoxelWorld&) = delete;

		int chunkSize() const { return size; }
		// The height the world was made with: the number of sections a column is held in, times the section size (VoxelBindings).
		int chunkHeight() const { return height; }

		void setColour(int blockId, glm::vec3 colour);
		// The surface style: 0 draws the blocks as cubes (greedy-merged faces); n >= 1 draws a smooth skin over them with n
		// samples per block (see buildSmoothMesh). Changing it re-meshes every loaded section.
		void setSmoothing(int resolution);
		int smoothing() const { return smoothResolution; }
		// Level of detail for the smooth skin: the sections within nearChunks of the focus (see update) use nearSamples per
		// block, the rest use the smoothing resolution. nearChunks 0 turns it off.
		void setDetail(int nearChunks, int nearSamples);
		// The ghost of a placement, in the runtime mesh "voxel_preview": the mesh a block would have at the cell, shown only where it
		// is new (outside the terrain as it is). setPreviewProp shows a torch leaning in direction dir (PropDirs). clearPreview hides it.
		void setPreview(int x, int y, int z, int blockId);
		void setPreviewProp(int x, int y, int z, int dir);
		// The outward normal of the skin at a point (the gradient of skinDensity, negated), and the normal where a torch leans on the
		// block in direction dir from cell (x, y, z): the stick stands out along it.
		glm::vec3 skinNormal(double x, double y, double z) const;
		glm::vec3 propNormal(int x, int y, int z, int dir) const;
		void clearPreview();
		// Far skins: the sections more than farChunks from the focus are meshed with every stride-th block sampled (a stride of 2
		// is a quarter of the quads). 0 chunks turns it off.
		void setFar(int farChunks, int stride);
		int resolutionFor(int cx, int cz) const;
		int strideFor(int cx, int cz) const;
		// The stride of a section: the far stride, or DeepStride for a section well under the ground (see DeepDepth in VoxelWorld.cpp).
		int strideFor(int cx, int cy, int cz) const;
		// The quads in the loaded sections of a column's mesh as last uploaded (0 if none), all groups together.
		int chunkQuads(int cx, int cz) const;
		// The quads of each mesh group the loaded sections of a column have, by group: 0 is the terrain, else the block id with its own mesh.
		std::map<int, int> chunkGroupQuads(int cx, int cz) const;
		// The same, for the sections of the column whose blocks lie in [yFrom, yTo): a column is a stack of sections, and the quads
		// of the stack below the ground (stone under a cleared floor) are not the ones in a range above it.
		std::map<int, int> chunkGroupQuads(int cx, int cz, int yFrom, int yTo) const;
		// The generator's settings. Sections requested later use them; loaded sections keep what they have.
		void setTerrain(const VoxelTerrain& settings) { terrain = settings; }
		/// The biome of the column at world (x, z): "plains", "forest", "desert" or "sea" (see VoxelTerrain::biomeAt).
		const char* biomeName(int x, int z) const;

		int getBlock(int x, int y, int z) const;
		// Sets a block, and queues it and its neighbours for the simulation. Unloaded sections keep the edit for later.
		void setBlock(int x, int y, int z, int blockId);
		// A torch: a prop in the air cell (x, y, z), leaning on the block in direction dir (VoxelMesher.h PropDirs, 0 to 5). It
		// gives light and is drawn, but it is not a block. False if the cell is a block or its section is not loaded.
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
		// Heals the block at (x, y, z) by one stage. Returns the stage it is at now (0 when healed fully or never damaged).
		int healBlock(int x, int y, int z);
		// The damage stage of the block at (x, y, z), 0 for none.
		int damageAt(int x, int y, int z) const;
		// Takes up to want units from the volume at (x, y, z) and the volume cells of the same liquid around it, nearest first,
		// as a bucket does. Returns the units it took and the cells it changed. With commit false it only counts (the cells are
		// not changed), so a bucket can check there is enough before it takes any.
		std::pair<int, std::vector<std::array<int, 3>>> drainVolume(int x, int y, int z, int want, bool commit);
		// Puts up to units of the liquid whose one unit is unitOne into the air at (x, y, z) and the volume cells around it (nearest
		// first, each cell up to a full volume), as a bucket pours. Returns the units it placed and the cells it changed. With commit
		// false it only counts. A caller that needs all of them placed checks that the count is units first.
		std::pair<int, std::vector<std::array<int, 3>>> pourVolume(int x, int y, int z, int units, int unitOne, bool commit);
		// Every prop as (x, y, z, dir). Props are kept with the edits, so a section that unloads keeps them.
		std::vector<std::array<int, 4>> props() const;
		// True if the column at (x, z) is in a loaded section, so its blocks are real.
		bool loadedAt(int x, int z) const;

		// What the blocks do (see VoxelBlocks.h): solid for collision, falling, or liquid, and how they are drawn (own, cube).
		// Set it for any id, once. A cube block is always drawn in its own mesh, so cube implies own.
		void defineBlock(int blockId, const BlockType& type);
		const BlockType& blockType(int blockId) const { return types[static_cast<size_t>(std::clamp(blockId, 0, 255))]; }
		bool isSolid(int x, int y, int z) const;
		// Whether the solid blocks around (x, y, z) hang from the ground (roadmap 39.1). Call it after the block at (x, y, z) has gone (set
		// to air). A flood fill over solid face neighbours from the solid blocks beside the cell: status 0 when it reaches the ground
		// (y = 0) or the edge of the loaded world, 1 when it runs out (a piece that is no longer connected, which would fall), 2 when it
		// stops at maxBlocks. visited is the number of blocks the fill took in.
		struct SupportResult {
			int status = 0;
			int visited = 0;
		};
		SupportResult checkSupport(int x, int y, int z, int maxBlocks) const;
		// The same fill from the given blocks (each solid). With cells not null, a result of status 1 fills it with every block the fill
		// took in: the detached piece.
		SupportResult checkSupportFrom(const std::vector<std::array<int, 3>>& seeds, int maxBlocks,
			std::vector<std::array<int, 3>>* cells) const;
		// Detached pieces (roadmap 39.1): when a block is broken, the solid blocks it held up that no longer reach the ground fall as one
		// piece (VoxelSimulation::detach). setDetachChecks(false) turns the look-up off, as a piece does while it moves.
		void setDetachChecks(bool on) { detachChecks = on; }
		bool detachChecksOn() const { return detachChecks; }
		int fallingPieces() const { return simulation.pieceCount(); }
		int fallingCells() const { return simulation.pieceCellCount(); }
		// The material a block's own mesh is drawn with (BlockType::own). Blocks without one use the world's material.
		void setMaterial(int blockId, const std::string& name);

		// The skins that collision sees (see skinDensity): 0 is air, TerrainSkin the terrain, OwnSkin + id a smooth block's own
		// skin and CubeSkin + id a cube block's. A solid block is in exactly one.
		static constexpr int TerrainSkin = 1;
		static constexpr int OwnSkin = 256;
		static constexpr int CubeSkin = 512;
		int skinAt(int x, int y, int z) const;
		// The same as skinAt, quicker for the common block: a solid terrain block is answered from its type alone.
		int skinFast(int x, int y, int z) const;
		// The terrain blocks among the 2 by 2 by 2 that meet at a block corner (0 to 8): the blocks x - 1 or x, and so on (VoxelTerrainShift).
		int terrainCorners(int x, int y, int z) const;
		// Fills the corner counts of the loaded section (cx, cy, cz).
		void buildCounts(int cx, int cy, int cz) const;

		/// The sky light at a block, 0 (none) to 255 (full), as the mesh shades it. Computed on demand, so this is for queries.
		uint8_t lightAt(int x, int y, int z) const;
		// Moves a box of this shape through the blocks (see VoxelBody.h). The C++ twin of VoxelPhysics.step.
		VoxelBodyState stepBody(const VoxelBodyState& body, const VoxelBodyShape& shape, double dt, bool flying) const;

		/// The skin's density at world position (x, y, z). Each smooth skin (the terrain, and each block type with its own mesh)
		/// is the occupancy of its blocks' centres around the point, weighted trilinearly; the density is the largest of them.
		/// A cube block's skin is 1 in its own cell and 0 outside. A point is inside the drawn surface where this is above
		/// VoxelSkinIso. Solid blocks occupy it (water and air do not), and cells of sections that are not loaded count as solid
		/// terrain, so a body cannot walk out of the loaded world.
		double skinDensity(double x, double y, double z) const;

		/// The first point along the ray from origin (direction is a unit vector) where the skin begins, within maxDistance. False
		/// if the origin is already inside the skin or nothing is hit. It is the surface the player sees under the crosshair.
		bool skinSurface(const std::array<double, 3>& origin, const std::array<double, 3>& direction, double maxDistance,
			std::array<double, 3>& surface) const;
		// The route a creature walks (VoxelPaths.find_path, in C++): from start to a cell within one block of goal (in x, y and z), breadth
		// first, a step to a neighbour on the same level, one up or one down, through standing cells (air with air above and ground under;
		// liquids do not hold). The cells after start, the goal last, in route; false when no route is found in the radius and cell cap.
		bool findRoute(const std::array<int, 3>& start, const std::array<int, 3>& goal, const std::vector<int>& liquids,
			std::vector<std::array<int, 3>>& route) const;
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
		// Sets the blocks from yFrom (inclusive) to yTo (exclusive).
		void fillColumn(int x, int z, int yFrom, int yTo, int blockId);

		// Starts generating the sections of column (cx, cz) on the workers. False if they are requested or loaded already.
		bool requestChunk(int cx, int cz);
		// True once every section of the column is installed.
		bool isLoaded(int cx, int cz) const;
		// Removes the column's sections and their bodies. The regions' neighbours are re-meshed, since their faces against them now show.
		void unloadChunk(int cx, int cz);
		// Installs finished terrain, uploads finished meshes until budgetMs have passed (at least one, if there is one),
		// and starts meshing up to a few dirty regions, nearest to the focus first. focusY is the block height of the focus (the
		// player's). Returns the meshes uploaded.
		int update(double budgetMs, int focusX, int focusZ, int focusY = 0);

		int chunkCount() const { return static_cast<int>(chunks.size()); }
		int loadedCount() const;
		// The loaded, pending, dirty sections and the quads, in one pass (what stats reports).
		struct Counts {
			int loaded = 0;
			int pending = 0;
			int dirty = 0;
			int quads = 0;
		};
		Counts counts() const;
		int pendingCount() const;       // sections waiting for terrain
		// The sections in the square of this radius around (cx, cz), in every height, whose mesh is up to date and uploaded (loaded, not
		// dirty, with a mesh, and its region not being meshed): what the player sees there. Measures how fast the ground arrives (roadmap 22.1).
		std::vector<int> readyCounts(int cx, int cz, const std::vector<int>& radii) const;
		// Generates the sections of a square of columns (side x side, from the origin) through the terrain, as the loader does. Returns the time in ms and
		// a checksum of the blocks, so a faster generator can be checked to give the same terrain. For the benchmark (tools/voxel_terrain_bench.py).
		std::pair<double, uint64_t> benchTerrain(int side, int bandLow, int bandHigh) const;
		int dirtyCount() const;         // loaded sections whose mesh is out of date
		int meshingCount() const;       // sections being meshed on a worker
		int quadCount() const;          // quads in the loaded sections' meshes, as last uploaded
		double lastUploadMs() const { return uploadMs; }
		// Where the time goes, cumulative since the world was made (roadmap 22.1): installing the terrain that arrived, starting the
		// meshes, the terrain generated and the meshes built on the workers, in milliseconds.
		struct Timings {
			double install = 0.0;
			double start = 0.0;
			double terrain = 0.0;
			double mesh = 0.0;
			double light = 0.0;
			double skin = 0.0;
			double surface = 0.0;
			double builds = 0.0;            // sections meshed, cumulative: a section is meshed again each time a neighbour changes
		};
		Timings timings() const { return totals; }
		// The time of each phase of the last update, in ms: [0] the band and the focus, [1] visibility, [2] the terrain installed, [3] the uploads,
		// [4] the mesh starts (their sources are copied here, on this thread; the snapshots are built on the workers), [5] the scan of the
			// dirty regions that settle to be started (before the starts).
		std::array<double, 6> lastPhases{};
		// Regions uploaded so far: it rises by one each time a region's mesh is shown (see uploadMesh).
		int uploadCount() const { return uploads; }
		// Removes every section and its body. Work still on the workers is dropped when it finishes. The edits stay.
		void clear();

		// Block edits are kept apart from the sections, so they survive unloading and are applied again when a section loads.
		// The file is binary: "CGVE", a version (1 to 3), a count, then per edit the int32 x, y, z and the uint8 block id.
		bool saveEdits(const std::string& path) const;
		// Adds the file's edits to the world's (a block edited in both takes the file's value). False if the file is unreadable.
		bool loadEdits(const std::string& path);
		int editCount() const;
		void clearEdits() { edits.clear(); }
		// The clouds (roadmap 45.2): a sheet of translucent quads at CloudY over the blocks around the focus, drifting with the time. It is
		// rebuilt about twice a second, so the clouds move smoothly; a call in between does nothing.
		void updateClouds(double timeSeconds, int focusX, int focusZ);
		// Records the cells that change from now on (blocks, torches and damage), for the host to send (VoxelNet). Off by default.
		void setRecordChanges(bool on);
		// The cells changed since the last call, as (x, y, z), each once. Forgets them.
		std::vector<std::array<int, 3>> takeChanges();
		// The falling blocks that began since the last call, while recording is on (the host sends them to its client).
		std::vector<VoxelSimulation::Move> takeMoveStarts() { return simulation.takeStarted(); }
		// A falling block another process started (see VoxelSimulation::showMove), and the advance of the moves in flight.
		void showMove(int fromX, int fromY, int fromZ, int toX, int toY, int toZ, uint8_t block, double seconds) {
			simulation.showMove(fromX, fromY, fromZ, toX, toY, toZ, block, seconds);
		}
		void advanceMoves(double dt) { simulation.advance(*this, dt); }
		// Every cell with an edit, a torch or damage, loaded or not, as (x, y, z).
		std::vector<std::array<int, 3>> editedCells() const;
		// Sets the damage stage of the block at (x, y, z); 0 clears it. A client takes the stage a host sends this way.
		void setDamage(int x, int y, int z, int stage);

	private:
		using Key = std::array<int, 3>;   // a section's (cx, cy, cz), or a region's (rx, ry, rz)

		bool detachChecks = true;
		// Called for a block broken at (x, y, z): every solid block beside it that no longer reaches the ground starts a falling piece.
		void detachAround(int x, int y, int z);
		void setBlockQuiet(int x, int y, int z, int blockId);
		struct Chunk {
			std::vector<uint8_t> blocks;    // size^3 blocks, (ly * size + lz) * size + lx; empty while the section is uniform (then every block is fill)
			uint8_t fill = 0;               // the block of every cell of a uniform section (see blocks)
			bool loaded = false;
			uint32_t generation = 0;        // unique per section, so late results for an unloaded section are ignored
			int quads = 0;                  // all groups together, as last uploaded
			int resolution = 0;             // the smooth skin's samples per block for this section (0: cubes); see resolutionFor
			int stride = 1;                 // the sample grid's stride for this section (2 for a far section); see strideFor
			bool deep = false;              // lies deep under the surface (its stride is DeepStride); set at install, it does not depend on the focus
			std::map<int, int> groupQuads;  // the quads of each group's mesh as last uploaded
			// The terrain's corner values (terrainCorners) for the section's corners: its own blocks' corners, x, y and z in [0, size].
			// Empty until it is needed; a change to the section or a neighbour clears it (markDirty).
			mutable std::vector<uint8_t> counts;
			bool dirty = true;                       // its blocks changed since its mesh was built, so it is meshed again (see markDirty)
			std::shared_ptr<const ChunkMesh> mesh;   // its meshes as last built, reused by a region job while it is not dirty
			// Which faces the air of the section connects (occlusion culling, see updateVisibility): bit g of reach[f] is set when the
			// air touching face f also touches face g. Faces: 0 +x, 1 -x, 2 +y, 3 -y, 4 +z, 5 -z. All connected until it is computed.
			std::array<uint8_t, 6> reach{ { 63, 63, 63, 63, 63, 63 } };
			uint32_t visitEpoch = 0;                 // the last visibility walk that reached this section (see updateVisibility)
		};

		// A region: the bodies that draw its sections, one per mesh group (0: the terrain). It is meshed as a whole.
		struct Region {
			std::map<int, id_t> bodies;     // the body of each group drawn, by its asset id
			bool meshing = false;           // a mesh is on a worker
			uint32_t epoch = 0;             // bumped when the bodies are removed, so a mesh made before that is dropped
			int settle = 0;                 // updates since the last change; see RegionSettleUpdates in VoxelWorld.cpp
			bool visible = true;            // drawn: some section of the region is reached from the camera (see updateVisibility)
		};

		struct TerrainResult {
			int cx;
			int cy;
			int cz;
			uint32_t generation;
			std::vector<uint8_t> blocks;
			double ms = 0.0;    // how long the worker took
		};

		// One group's mesh of a section (see buildChunkMesh): group 0 is the terrain.
		struct GroupMesh {
			uint8_t group = 0;
			MeshArrays mesh;
			int quads = 0;
		};

		// A section as a region job left it: its mesh, which the section keeps, and the generation it was meshed for (see Chunk).
		struct ChunkBuilt {
			int cx;
			int cy;
			int cz;
			uint32_t generation;
			std::shared_ptr<const ChunkMesh> mesh;
			std::array<uint8_t, 6> reach{};   // the occlusion faces, when the job built them (reachKnown; see Chunk::reach)
			bool reachKnown = false;
		};

		// A section's blocks as a snapshot reads them, copied on the world's thread (see gather): its own blocks, or for a section beyond the
		// band the terrain to be generated on the worker, with its edits.
		struct SectionSource {
			enum class Kind : uint8_t { Absent, Loaded, Analytic };
			Kind kind = Kind::Absent;
			uint8_t fill = 0;
			std::vector<uint8_t> blocks;    // empty while the section is uniform (then every block is fill)
			std::map<int, uint8_t> edits;   // Analytic: the edits applied over the generated terrain
		};
		// A prop, or a damaged block, in world coordinates (see PaddedChunk::props and PaddedChunk::damage).
		struct PropCopy {
			int x;
			int y;
			int z;
			uint8_t dir;
			float anchor;
			glm::vec3 normal;
		};
		struct DamageCopy {
			int x;
			int y;
			int z;
			uint8_t value;
		};
		// Everything a snapshot needs from the world, copied: the sections of a box around the ones meshed, the column tops, the props, the
		// damage and the block rules. A worker builds the snapshot from it, so it never reads the world while the world changes.
		struct SnapshotSource {
			int size = 16;
			int border = 1;
			Key origin = { 0, 0, 0 };                // the first section gathered
			std::array<int, 3> dims = { 1, 1, 1 };   // the sections gathered, from origin
			std::vector<SectionSource> sections;     // by ((dy * dims[2]) + dz) * dims[0] + dx
			int topX = 0;                            // the column grid of the tops: its first column (x and z), and its width in columns
			int topZ = 0;
			int topWidth = 0;
			std::vector<int> tops;                   // tops[(z - topZ) * topWidth + (x - topX)], as topAt gives them
			std::vector<PropCopy> props;
			std::vector<DamageCopy> damage;
			std::array<uint8_t, 256> emission{};
			std::array<bool, 256> passesLight{};
			std::array<PaddedChunk::MeshRule, 256> rule{};
			VoxelTerrain terrain;                    // the settings the analytic sections are generated from
			const SectionSource& section(const Key& key) const {
				const int dx = key[0] - origin[0], dy = key[1] - origin[1], dz = key[2] - origin[2];
				return sections[static_cast<size_t>((dy * dims[2] + dz) * dims[0] + dx)];
			}
		};

		struct RegionResult {
			Key region;                     // (rx, ry, rz)
			uint32_t epoch;
			std::vector<GroupMesh> groups;  // each group's mesh for the whole region, sections merged
			std::vector<ChunkBuilt> chunks;
			double ms = 0.0;                // how long the worker took
			double lightMs = 0.0;           // of which: the light, the skin and the surfaces (the mesher's stages)
			double skinMs = 0.0;
			double surfaceMs = 0.0;
			int built = 0;                  // sections this job built the mesh of (the others were reused)
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
		int farRadius = 0;              // sections farther than this from the focus are far skins (setFar)
		int farStride = 2;
		// The cloud sheet's body, when it is shown (see updateClouds), and when it was last built.
		std::pair<bool, id_t> cloudBody = { false, 0 };
		double cloudBuiltAt = -1000.0;
		int focusCx = 0;
		int focusCz = 0;
		int focusCy = 0;
		bool focusKnown = false;
		// The blocks in flight, drawn as one mesh of balls (see updateMovingBlocks).
		std::pair<bool, id_t> movingBody = { false, 0 };
		// The placement ghost's body, if one is shown (see setPreview).
		std::pair<bool, id_t> previewBody = { false, 0 };
		bool movingShown = false;
		std::map<Key, Chunk> chunks;
		// The highest solid block of each loaded column (x, z) in each of its cells: NoSolid where there is none. Light reads it to know
		// whether the sky is open above a section (PaddedChunk::sky), so a section under a roof is dark.
		static constexpr int NoSolid = -(1 << 30);
		std::map<std::pair<int, int>, std::vector<int>> tops;
		// The regions whose mesh may be out of date, so update() finds them without scanning every region.
		std::set<Key> dirtyRegions;
		std::map<Key, Region> regions;
		// Edited blocks by section: the section's key -> local index (localIndex) -> block id. See setBlock. They are kept whether or not
		// the section is loaded, so an edit is in place when the section loads, and beyond the band it is read over the terrain.
		std::map<Key, std::map<int, uint8_t>> edits;
		// Props (torches) by section: local index -> dir. See setProp.
		std::map<Key, std::map<int, uint8_t>> propsBySection;
		// Damage by section: local index -> stage (see hitBlock). Saved with the edits (version 3 of the edit file).
		std::map<Key, std::map<int, uint8_t>> damageBySection;
		// The columns requested (see requestChunk): the band of sections of each one is wanted. Their sections load and unload with the band.
		std::set<std::pair<int, int>> columns;
		// The band is reconciled once the focus has been seen (see update).
		bool bandKnown = false;
		// The band window (bandLow..bandHigh) as the last reconcile left it: the next one touches only the sections that entered or left it.
		bool windowKnown = false;
		int reconciledLow = 0;
		// The level (resolution, horizontal stride) each wanted column's sections were last given (see refreshResolutions).
		std::map<std::pair<int, int>, std::pair<int, int>> columnClasses;
		int reconciledHigh = 0;
		// The sections this far (in sections) from the focus's section, up or down, are loaded: the band (see inBand).
		int bandSections = 6;
		// A loaded section stays until it is this many sections beyond the band (the hysteresis of reconcileBand).
		static constexpr int BandKeep = 2;
		// Cells changed while recordChanges is on (see takeChanges).
		bool recordChanges = false;
		std::set<std::array<int, 3>> changedCells;
		void noteChange(int x, int y, int z) { if (recordChanges) changedCells.insert(std::array<int, 3>{ x, y, z }); }
		void clearDamage(int x, int y, int z);
		uint32_t nextGeneration = 1;
		double uploadMs = 0.0;
		Timings totals;
		int uploads = 0;

		// Background work. Jobs run on the workers; their results wait in the result queues until update() takes them.
		std::vector<std::thread> workers;
		std::mutex jobMutex;
		std::condition_variable jobReady;
		// A queued job and the section it is for (a region's mesh job uses its middle section). The job nearest the focus runs first, so
		// the ground the player is on is meshed before far terrain queued earlier. unloadChunk drops a section's queued terrain.
		struct Job {
			int x = 0;
			int y = 0;
			int z = 0;
			bool terrain = false;
			std::function<void()> run;
		};
		std::vector<Job> jobs;
		int jobFocusX = 0;              // the section the job order is measured from, set by update() (setJobFocus)
		int jobFocusZ = 0;
		bool stopping = false;
		std::mutex resultMutex;
		std::deque<TerrainResult> terrainResults;
		std::deque<RegionResult> meshResults;

		void startWorkers();
		// Re-reads the detail of every loaded section for a new focus; a section whose level changed is meshed again.
		void refreshResolutions(int focusX, int focusZ, bool force);
		void updateMovingBlocks();
		// The ghost's mesh for a block (or a torch when propDir is 0 to 5), in world coordinates, with only its new triangles.
		void previewMesh(int x, int y, int z, int blockId, int propDir, MeshArrays& out) const;
		void showPreview(MeshArrays& arrays);
		void stopWorkers();
		void workerLoop();
		void enqueue(int x, int y, int z, bool terrain, std::function<void()> job);
		void setJobFocus(int x, int z);

		static int floorDiv(int a, int b);
		// Index of a block in its section's blocks: (ly * size + lz) * size + lx, with each local coordinate in [0, size).
		int localIndex(int lx, int ly, int lz) const { return (ly * size + lz) * size + lx; }
		// The section holding world block (x, y, z), as its key.
		Key sectionOf(int x, int y, int z) const { return { floorDiv(x, size), floorDiv(y, size), floorDiv(z, size) }; }
		// The block at local (lx, ly, lz) of a section, which may be uniform.
		static uint8_t blockIn(const Chunk& chunk, int index) { return chunk.blocks.empty() ? chunk.fill : chunk.blocks[static_cast<size_t>(index)]; }
		const Chunk* findChunk(const Key& key) const;
		// The section the last lookup found: a block or a density query reads the same section many times in a row, and the map
		// lookup is what costs. Erasing a section clears it (see unloadChunk and clear).
		mutable Key memoKey = { 0, 0, 0 };
		mutable const Chunk* memoChunk = nullptr;
		Chunk* findChunk(const Key& key);
		// The column's top solid at (x, z), as a loaded section's blocks give it (see tops), or NoSolid.
		int topAt(int x, int z) const;
		// Records the highest roof (a block light does not pass) of the section's blocks in its column, after the section is installed.
		void updateTops(const Key& key, const Chunk& chunk);
		// A block changed at (x, y, z): the column's top moves when a roof goes in above it, or when the top roof comes out.
		void changeTop(int x, int y, int z, uint8_t block);
		// The column's tops from the loaded sections again (a section left the world, and took its roofs with it).
		void recomputeTops(int cx, int cz);
		// The sections of the 3 by 3 columns around (cx, cz) are meshed again: a column's sky changed.
		void markTopChanged(int cx, int cz);
		// Whether a block is a roof (see tops): anything light does not pass.
		bool roofs(uint8_t block) const;
		// Whether the section holding world block (x, y, z) is loaded.
		bool sectionLoaded(int x, int y, int z) const;
		// Sets one block of a loaded section, expanding a uniform section to its array first.
		void writeBlock(Chunk& chunk, int index, uint8_t block);
		// Stores a section's blocks: as the array, or as the one id when they are all the same.
		static void storeBlocks(Chunk& chunk, std::vector<uint8_t>&& blocks);
		void markDirty(const Key& key);
		// Bumped whenever a section changes, so the visibility is worked out again (see updateVisibility).
		uint64_t visibilityStamp = 0;
		uint64_t visibilitySeen = ~uint64_t(0);
		int visibilitySkips = 0;   // updates a stamp-only change has waited for the walk (see updateVisibility)
		Key visibilityFrom = { 0, 0, 0 };
		uint32_t visitEpoch = 0;
		// Which sections the camera's section sees through open air, and which regions are drawn: an occluded region is hidden.
		void updateVisibility(int cx, int cy, int cz);
		// Marks the sections around the block at (x, y, z) that its light (and skin) reaches, as setBlock does.
		void markLight(int x, int y, int z);
		void markNeighboursDirty(const Key& key);
		// Marks these sections dirty, and their neighbours too when neighbours is set, as markDirty and markNeighboursDirty do, in one batch.
		void markSectionsDirty(const std::vector<Key>& keys, bool neighbours);
		// The region holding section key, as (rx, ry, rz).
		static Key regionOf(const Key& key);
		// Marks a region changed: it is meshed again once it has settled.
		void markRegionDirty(const Key& key);
		// The loaded sections in a region.
		int loadedChunksIn(const Key& key) const;
		// The loaded sections of a region, and whether the region is settled: none of them has a neighbour requested and still waiting for its
		// terrain (a section's mesh reads its neighbours, so it is meshed once they are in, not again each time one arrives; see update).
		void surveyRegion(const Key& key, int& loaded, bool& settled) const;
		// Removes a region's bodies. Its sections are not touched; the region is left clean and empty.
		void removeRegionBodies(const Key& key);
		void installTerrain(TerrainResult& result);
		void applyEdits(const Key& key, std::vector<uint8_t>& blocks) const;
		void uploadMesh(RegionResult& result);
		void startMeshing(const Key& key);
		PaddedChunk snapshot(int cx, int cy, int cz, int border) const;
		// The world's state around the sections origin + [0, dims), copied (see SnapshotSource). It is made on the world's thread.
		SnapshotSource gather(const Key& origin, const std::array<int, 3>& dims, int border) const;
		// The padded box of section (cx, cy, cz) from a gathered source. The analytic sections are generated once, into analytic.
		static PaddedChunk buildPadded(const SnapshotSource& source, int cx, int cy, int cz, std::map<Key, std::vector<uint8_t>>& analytic);
		// The band's half-height in sections (see bandSections), and the sections loaded beside it.
	public:
		// Sets how many sections the band reaches above and below the focus: the vertical view distance. The sections out of the new
		// band go at the next update, and the new ones are requested.
		void setBand(int sections) {
			const int next = std::max(1, sections);
			if (next == bandSections) return;
			bandSections = next;
			bandKnown = false;
			windowKnown = false;
		}
	private:
		// Whether a section at this height (in sections) is in the band, and the band's ends (in sections).
		bool inBand(int cy) const;
		bool withinBand(int y) const;
		bool wanted(int cx, int cz) const;
		int bandLow() const;
		int bandHigh() const;
		// Requests the band's sections of every wanted column, and drops the sections out of it.
		void reconcileBand();
		void requestSection(const Key& key);
		// Removes sections (see dropSections); a section that goes is gone from the world's sections, its tops and its region.
		void dropSection(const Key& key) { dropSections({ key }); }
		void dropSections(const std::vector<Key>& keys);
		// The block of an edit at (x, y, z), if there is one.
		bool editAt(int x, int y, int z, uint8_t& out) const;
		// The block of a cell beyond the band: its edit, else the terrain as the generator makes it (no trees, rocks or edits from other sections).
		uint8_t analyticBlock(int x, int y, int z) const;
	public:
		// The ground (group 0) smooth mesh of a section, built now the way a worker builds it, for the checks: its vertices.
		std::vector<VoxelVertex> probeGroundMesh(int cx, int cy, int cz, uint8_t group = 0, int stride = 1) const;
	private:
		std::string bodyName(const Key& region, uint8_t group) const;
	};
}
