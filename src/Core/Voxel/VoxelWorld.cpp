#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <unordered_set>
#include <chrono>
#include "VoxelWorld.h"
#include "../Engine/Engine.h"
#include "../Mesh/RuntimeMesh.h"
#include "../Body/Body.h"

namespace CGEngine {
	namespace {
		// Loaded chunks copied for meshing per update at most, so one update never copies too many of them.
		const int MaxMeshStartsPerUpdate = 16;
	// The placement ghost is built from the blocks this far each side of its cell, and keeps the surface this close to it.
	const int PreviewRadius = 4;
	const float PreviewKeep = 2.5f;

	// A smooth step from 0 at from to 1 at to (0 and 1 outside them).
	float smoothRamp(float from, float to, float x) {
		const float t = std::clamp((x - from) / (to - from), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// How far a ghost's vertex has risen out of the terrain: 0 on its surface, 1 a fifth of a block above it. The ghost fades in from
	// the ground it stands on, so it does not end on a hard line where it meets the terrain (roadmap 24.3).
	float ghostRise(const PaddedChunk& terrain, glm::vec3 p) {
		return smoothRamp(0.0f, 0.2f, VoxelSkinIso - densityAtPoint(terrain, 0, p));
	}
		// A region is RegionChunks by RegionChunks chunks, drawn as one body per mesh group. The view then draws a few
		// hundred regions' groups instead of a body per chunk (roadmap 15.5).
		const int RegionChunks = 4;
		// A changed region waits this many updates before it is meshed, so a run of changes (chunks streaming in beside
		// it) is meshed once.
		const int RegionSettleUpdates = 2;

		// One loaded chunk of a region, copied for its mesh: the blocks with their neighbours, and the chunk's detail.
		struct ChunkInput {
			int cx = 0;
			int cz = 0;
			int resolution = 0;
			int stride = 1;
			unsigned skirtSides = 0;
			uint32_t generation = 0;
			std::shared_ptr<const ChunkMesh> reuse;   // the mesh the chunk kept, when it has not changed since: then it is not meshed again
			PaddedChunk padded;
		};

		// Adds a chunk's mesh to a region's, moved by the chunk's place in the region (offsets from the region's corner).
		void appendMesh(MeshArrays& into, const MeshArrays& part, float offsetX, float offsetZ) {
			const unsigned int base = static_cast<unsigned int>(into.vertices.size());
			into.vertices.reserve(into.vertices.size() + part.vertices.size());
			for (VoxelVertex vertex : part.vertices) {
				vertex.position.x += offsetX;
				vertex.position.z += offsetZ;
				into.vertices.push_back(vertex);
			}
			into.indices.reserve(into.indices.size() + part.indices.size());
			for (unsigned int index : part.indices) into.indices.push_back(index + base);
		}
		const size_t MaxWorkers = 6;
	}

	VoxelWorld::VoxelWorld(int chunkSize, int chunkHeight, const std::string& materialName)
		: size(chunkSize), height(chunkHeight), material(materialName) {
		palette.fill(glm::vec3(1.0f));
		// The terrain's own blocks: sand falls, and water is a liquid that is not solid. Air is not solid either.
		types.fill(BlockType());
		types[0].solid = false;
		types[VoxelTerrain::Sand].falls = true;
		// Liquids are in their own mesh, drawn with their source's mesh, so the terrain's skin never counts them as ground (the game
		// sets the same in defineBlock; this is the default for a world that defines no blocks).
		const auto liquid = [](uint8_t source) {
			BlockType type{ false, false, true };
			type.own = true;
			type.mesh = source;
			type.blend = source == Water::Source;
			type.passesLight = source == Water::Source;
			return type;
		};
		// Each spring is its own mesh group's source (the water's group is 5, the lava's is LavaSpring): endless springs and the flows
		// share it with the producing spring, and the volumes are drawn in it too (see the game's define_block).
		types[Water::Source] = liquid(Water::Source);
		types[Water::Endless] = liquid(Water::Source);
		for (int id = Water::FlowFirst; id <= Water::FlowLast; ++id) types[static_cast<size_t>(id)] = liquid(Water::Source);
		types[LavaSpring] = liquid(LavaSpring);
		types[LavaEndless] = liquid(LavaSpring);
		for (int id = LavaFlow.first; id <= LavaFlow.last; ++id) types[static_cast<size_t>(id)] = liquid(LavaSpring);
		startWorkers();
	}

	std::vector<VoxelVertex> VoxelWorld::probeGroundMesh(int cx, int cz, uint8_t group, int stride) const {
		PaddedChunk padded = snapshot(cx, cz, VoxelLightReach);
		computeLight(padded);
		computeSkin(padded);
		MeshArrays part;
		buildSmoothMesh(padded, palette, std::max(1, resolutionFor(cx, cz)), std::max(1, stride), 0, group, groupBounds(padded)[group], part);
		return part.vertices;
	}

	void VoxelWorld::defineBlock(int blockId, const BlockType& type) {
		BlockType stored = type;
		if (stored.cube) stored.own = true;   // a cube is drawn in its own mesh (see BlockType)
		types[static_cast<size_t>(std::clamp(blockId, 0, 255))] = stored;
	}

	void VoxelWorld::setMaterial(int blockId, const std::string& name) {
		groupMaterials[static_cast<size_t>(std::clamp(blockId, 0, 255))] = name;
	}

	bool VoxelWorld::isSolid(int x, int y, int z) const {
		const int block = getBlock(x, y, z);
		return block != 0 && types[static_cast<size_t>(block)].solid;
	}

	VoxelWorld::SupportResult VoxelWorld::checkSupport(int x, int y, int z, int maxBlocks) const {
		static const int Dirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		std::vector<std::array<int, 3>> seeds;
		for (const auto& d : Dirs) {
			if (isSolid(x + d[0], y + d[1], z + d[2])) seeds.push_back({ x + d[0], y + d[1], z + d[2] });
		}
		return checkSupportFrom(seeds, maxBlocks, nullptr);
	}

	VoxelWorld::SupportResult VoxelWorld::checkSupportFrom(const std::vector<std::array<int, 3>>& seeds, int maxBlocks,
		std::vector<std::array<int, 3>>* cells) const {
		static const int Dirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		// A block is keyed by its coordinates, offset so each fits in its bits (the world is under 1024 blocks high).
		auto key = [](int bx, int by, int bz) {
			return (static_cast<uint64_t>(bx + (1 << 20)) << 42) | (static_cast<uint64_t>(by + (1 << 10)) << 21) | static_cast<uint64_t>(bz + (1 << 20));
		};
		std::unordered_set<uint64_t> seen;
		std::vector<std::array<int, 3>> open;
		std::vector<std::array<int, 3>> visited;
		for (const auto& seed : seeds) {
			if (!isSolid(seed[0], seed[1], seed[2])) continue;
			if (seen.insert(key(seed[0], seed[1], seed[2])).second) open.push_back(seed);
		}
		SupportResult result;
		while (!open.empty()) {
			const std::array<int, 3> block = open.back();
			open.pop_back();
			++result.visited;
			if (cells) visited.push_back(block);
			if (block[1] == 0) {
				result.status = 0;
				return result;
			}
			if (result.visited >= maxBlocks) {
				result.status = 2;
				return result;
			}
			for (const auto& d : Dirs) {
				const int bx = block[0] + d[0], by = block[1] + d[1], bz = block[2] + d[2];
				if (!loadedAt(bx, bz)) {
					result.status = 0;   // the edge of the loaded world: the piece is held by what is not loaded, so it is not checked further
					return result;
				}
				if (!isSolid(bx, by, bz)) continue;
				if (seen.insert(key(bx, by, bz)).second) open.push_back({ bx, by, bz });
			}
		}
		result.status = 1;
		if (cells) *cells = std::move(visited);
		return result;
	}

	void VoxelWorld::detachAround(int x, int y, int z) {
		static const int Dirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for (const auto& d : Dirs) {
			const int nx = x + d[0], ny = y + d[1], nz = z + d[2];
			if (!isSolid(nx, ny, nz) || simulation.inPiece(nx, ny, nz)) continue;
			std::vector<std::array<int, 3>> cells;
			const SupportResult result = checkSupportFrom({ { nx, ny, nz } }, 16384, &cells);
			if (result.status != 1) continue;
			std::vector<VoxelSimulation::PieceCell> piece;
			piece.reserve(cells.size());
			for (const auto& c : cells) piece.push_back({ c[0], c[1], c[2], static_cast<uint8_t>(getBlock(c[0], c[1], c[2])) });
			simulation.detach(std::move(piece));
		}
	}

	int VoxelWorld::skinFast(int x, int y, int z) const {
		if (y < 0 || y >= height || !loadedAt(x, z)) return skinAt(x, y, z);
		const int block = getBlock(x, y, z);
		if (block == 0) return 0;
		const BlockType& type = types[static_cast<size_t>(block)];
		if (!type.solid) return 0;
		if (!type.own) return TerrainSkin;
		return skinAt(x, y, z);
	}

	int VoxelWorld::skinAt(int x, int y, int z) const {
		if (y < 0) return TerrainSkin;                  // the world's floor is solid
		if (y >= height) return 0;
		if (!loadedAt(x, z)) return TerrainSkin;        // unloaded cells are solid, so a body stays in the loaded world
		const int block = getBlock(x, y, z);
		if (block == 0) return 0;
		const BlockType& type = types[static_cast<size_t>(block)];
		if (!type.solid) return 0;
		if (!type.own) return TerrainSkin;
		// A block whose top is covered by its join mesh is in that mesh's skin (the same rule as the mesher's inGroup).
		uint8_t mesh = meshGroup(type, static_cast<uint8_t>(block));
		bool joinedAbove = false;
		if (type.joinAbove && y + 1 < height) {
			const int above = getBlock(x, y + 1, z);
			if (above != 0 && meshGroup(types[static_cast<size_t>(above)], static_cast<uint8_t>(above)) == type.joinAbove) {
				mesh = type.joinAbove;
				joinedAbove = true;
			}
		}
		// A block standing on a block of its join-below set is in the terrain's skin (the same rule as the mesher's inGroup).
		if (!joinedAbove && type.joinBelow.any() && y - 1 >= 0 && type.joinBelow[static_cast<size_t>(getBlock(x, y - 1, z))]) {
			return TerrainSkin;
		}
		return (type.cube ? CubeSkin : OwnSkin) + mesh;
	}

	const char* VoxelWorld::biomeName(int x, int z) const {
		switch (terrain.biomeAt(x, z)) {
		case VoxelTerrain::Biome::Forest: return "forest";
		case VoxelTerrain::Biome::Desert: return "desert";
		default: return "plains";
		}
	}

	uint8_t VoxelWorld::lightAt(int x, int y, int z) const {
		if (y < 0) return 0;
		if (y >= height) return 255;
		// Built on demand from the blocks around the cell, so it is a query, not the mesher's path. It is exact for a loaded
		// chunk; the sky is taken to be open where a neighbour is not loaded.
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		PaddedChunk padded = snapshot(cx, cz, VoxelLightReach);
		computeLight(padded);
		const int lx = x - cx * size;
		const int lz = z - cz * size;
		return std::max(padded.lightAt(lx, y, lz), padded.lampAt(lx, y, lz));
	}

	int VoxelWorld::terrainCorners(int x, int y, int z) const {
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		const Chunk* chunk = findChunk(cx, cz);
		if (y < 0 || y > height || !chunk || !chunk->loaded) {
			// The floor, the air above and the unloaded chunks count as skinAt sees them.
			int total = 0;
			for (int dz = 0; dz <= 1; ++dz) {
				for (int dy = 0; dy <= 1; ++dy) {
					for (int dx = 0; dx <= 1; ++dx) {
						if (skinAt(x - 1 + dx, y - 1 + dy, z - 1 + dz) == TerrainSkin) ++total;
					}
				}
			}
			return total;
		}
		if (chunk->counts.empty()) buildCounts(cx, cz);
		return chunk->counts[(static_cast<size_t>(y) * size + (z - cz * size)) * size + (x - cx * size)];
	}

	void VoxelWorld::buildCounts(int cx, int cz) const {
		const Chunk* chunk = findChunk(cx, cz);
		if (!chunk) return;
		// The terrain occupancy of the blocks one before the chunk's corners and up to its top, then the 2 by 2 by 2 sum at each corner.
		const int U = size + 1;
		const int W = size + 1;
		const int V = height + 2;
		std::vector<uint8_t> solid(static_cast<size_t>(U) * W * V, 0);
		for (int v = 0; v < V; ++v) {
			for (int w = 0; w < W; ++w) {
				for (int u = 0; u < U; ++u) {
					solid[(static_cast<size_t>(v) * W + w) * U + u] = skinFast(cx * size + u - 1, v - 1, cz * size + w - 1) == TerrainSkin ? 1 : 0;
				}
			}
		}
		chunk->counts.assign(static_cast<size_t>(size) * size * (height + 1), 0);
		for (int j = 0; j <= height; ++j) {
			for (int k = 0; k < size; ++k) {
				for (int i = 0; i < size; ++i) {
					int total = 0;
					for (int dv = 0; dv <= 1; ++dv) {
						for (int dw = 0; dw <= 1; ++dw) {
							for (int du = 0; du <= 1; ++du) {
								total += solid[(static_cast<size_t>(j + dv) * W + (k + dw)) * U + (i + du)];
							}
						}
					}
					chunk->counts[(static_cast<size_t>(j) * size + k) * size + i] = static_cast<uint8_t>(total);
				}
			}
		}
	}


	double VoxelWorld::skinDensity(double x, double y, double z) const {
		// The same weights as the mesh's density (VoxelMesher.cpp), per skin (see skinAt). The terrain's density is the larger of its
		// share of the 2 by 2 by 2 blocks at each block corner, less VoxelTerrainShift, interpolated between the corners, and its block
		// centres' occupancy, scaled by VoxelCentreWeight (the mesh's corners and centres are the same numbers). Each other smooth skin adds up the weights of the corners it is in, and the density is the largest. The
		// world's ends are solid below and open above, and the unloaded cells are solid terrain. Python's copy (VoxelPhysics.py)
		// matches it operation for operation, so the parity check can compare them.
		// The common case, read straight: the 3 by 3 by 3 blocks the density reads lie in one loaded chunk, away from its walls and
		// floor and roof, and none is a smooth block. Then the terrain is the chunk's corner counts and its blocks, which is the sum below
		// with the same numbers (the creatures' and the player's collision queries are this function, so it is the hot path).
		{
			const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y)), iz = static_cast<int>(std::floor(z));
			const int cx = floorDiv(ix, size), cz = floorDiv(iz, size);
			const int lx = ix - cx * size, lz = iz - cz * size;
			const Chunk* chunk = findChunk(cx, cz);
			if (chunk && chunk->loaded && lx >= 1 && lx <= size - 2 && lz >= 1 && lz <= size - 2 && iy >= 1 && iy <= height - 2) {
				bool smooth = false;
				for (int dz = -1; dz <= 1 && !smooth; ++dz) {
					for (int dy = -1; dy <= 1 && !smooth; ++dy) {
						for (int dx = -1; dx <= 1 && !smooth; ++dx) {
							smooth = types[chunk->blocks[localIndex(lx + dx, iy + dy, lz + dz)]].own;
						}
					}
				}
				if (!smooth) {
					if (chunk->counts.empty()) buildCounts(cx, cz);
					// The centre cells are the block whose centre is the corner below the point, as in the sum below.
					const double bxFloor = std::floor(x - 0.5), byFloor = std::floor(y - 0.5), bzFloor = std::floor(z - 0.5);
					const int ox = static_cast<int>(bxFloor) - cx * size, oy = static_cast<int>(byFloor), oz = static_cast<int>(bzFloor) - cz * size;
					const double ux = x - ix, uy = y - iy, uz = z - iz;
					const double tx = x - 0.5 - bxFloor, ty = y - 0.5 - byFloor, tz = z - 0.5 - bzFloor;
					double terrainShare = 0.0;
					double centres = 0.0;
					for (int c = 0; c < 8; ++c) {
						const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
						const double w = (dx ? ux : 1.0 - ux) * (dy ? uy : 1.0 - uy) * (dz ? uz : 1.0 - uz);
						const size_t corner = (static_cast<size_t>(iy + dy) * size + (lz + dz)) * size + (lx + dx);
						terrainShare += w * (chunk->counts[corner] / 8.0 - VoxelTerrainShift);
						const double wc = (dx ? tx : 1.0 - tx) * (dy ? ty : 1.0 - ty) * (dz ? tz : 1.0 - tz);
						const uint8_t block = chunk->blocks[localIndex(ox + dx, oy + dy, oz + dz)];
						if (block != 0 && types[block].solid) centres += wc;
					}
					return std::max(terrainShare, VoxelCentreWeight * centres);
				}
			}
		}
		const double bx = x - 0.5, by = y - 0.5, bz = z - 0.5;
		const double fx = std::floor(bx), fy = std::floor(by), fz = std::floor(bz);
		const double tx = bx - fx, ty = by - fy, tz = bz - fz;
		const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy), z0 = static_cast<int>(fz);
		int skin[8];
		double weight[8];
		for (int c = 0; c < 8; ++c) {
			const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
			weight[c] = (dx ? tx : 1.0 - tx) * (dy ? ty : 1.0 - ty) * (dz ? tz : 1.0 - tz);
			skin[c] = skinFast(x0 + dx, y0 + dy, z0 + dz);
		}
		// The terrain, interpolated between the block corners: the corner at or below the point, then the next one up.
		const double tcx = std::floor(x), tcy = std::floor(y), tcz = std::floor(z);
		const double ux = x - tcx, uy = y - tcy, uz = z - tcz;
		const int c0x = static_cast<int>(tcx), c0y = static_cast<int>(tcy), c0z = static_cast<int>(tcz);
		double terrain = 0.0;
		for (int c = 0; c < 8; ++c) {
			const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
			const double w = (dx ? ux : 1.0 - ux) * (dy ? uy : 1.0 - uy) * (dz ? uz : 1.0 - uz);
			terrain += w * (terrainCorners(c0x + dx, c0y + dy, c0z + dz) / 8.0 - VoxelTerrainShift);
		}
		// The terrain's block centres, weighted as the other skins below (VoxelCentreWeight): a thin terrain feature keeps its width.
		double centres = 0.0;
		for (int c = 0; c < 8; ++c) {
			if (skin[c] == TerrainSkin) centres += weight[c];
		}
		double value = std::max(terrain, VoxelCentreWeight * centres);
		for (int c = 0; c < 8; ++c) {
			if (skin[c] == 0 || skin[c] == TerrainSkin || skin[c] >= CubeSkin) continue;
			// Each smooth skin is summed once, at its first corner.
			bool first = true;
			for (int earlier = 0; earlier < c; ++earlier) {
				if (skin[earlier] == skin[c]) first = false;
			}
			if (!first) continue;
			double sum = 0.0;
			for (int d = 0; d < 8; ++d) {
				if (skin[d] == skin[c]) sum += weight[d];
			}
			value = std::max(value, sum);
		}
		// A cube block's skin is the cell the point is in: full there, nothing outside it.
		const int cell = skinAt(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(std::floor(z)));
		if (cell >= CubeSkin) value = std::max(value, 1.0);
		return value;
	}

	bool VoxelWorld::skinSurface(const std::array<double, 3>& origin, const std::array<double, 3>& direction, double maxDistance,
		std::array<double, 3>& surface) const {
		const auto at = [&](double t) {
			return std::array<double, 3>{ origin[0] + direction[0] * t, origin[1] + direction[1] * t, origin[2] + direction[2] * t };
		};
		const auto inside = [&](double t) {
			const auto p = at(t);
			return skinDensity(p[0], p[1], p[2]) > VoxelSkinIso;
		};
		if (inside(0.0)) return false;
		const double step = 0.05;
		double outside = 0.0;
		for (double t = step; t <= maxDistance + step; t += step) {
			if (!inside(t)) {
				outside = t;
				continue;
			}
			// The surface lies between the last point outside and this one: bisect it.
			double free = outside;
			double blocked = t;
			for (int i = 0; i < 12; ++i) {
				const double mid = (free + blocked) * 0.5;
				if (inside(mid)) {
					blocked = mid;
				} else {
					free = mid;
				}
			}
			surface = at(free);
			return true;
		}
		return false;
	}

	bool VoxelWorld::loadedAt(int x, int z) const {
		const Chunk* chunk = findChunk(floorDiv(x, size), floorDiv(z, size));
		return chunk && chunk->loaded;
	}

	VoxelWorld::~VoxelWorld() {
		stopWorkers();
	}

	void VoxelWorld::startWorkers() {
		// One core is left for the main thread (the game and the renderer).
		const unsigned int cores = std::thread::hardware_concurrency();
		const size_t count = std::clamp(cores > 1 ? static_cast<size_t>(cores - 1) : size_t(2), size_t(1), MaxWorkers);
		for (size_t i = 0; i < count; ++i) {
			workers.emplace_back([this] { workerLoop(); });
		}
	}

	void VoxelWorld::stopWorkers() {
		{
			std::lock_guard<std::mutex> lock(jobMutex);
			stopping = true;
			jobs.clear();
		}
		jobReady.notify_all();
		for (std::thread& worker : workers) {
			if (worker.joinable()) worker.join();
		}
		workers.clear();
	}

	void VoxelWorld::workerLoop() {
		for (;;) {
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> lock(jobMutex);
				jobReady.wait(lock, [this] { return stopping || !jobs.empty(); });
				if (stopping) return;
				size_t nearest = 0;
				long long best = -1;
				for (size_t i = 0; i < jobs.size(); ++i) {
					const long long dx = jobs[i].x - jobFocusX;
					const long long dz = jobs[i].z - jobFocusZ;
					const long long distance = dx * dx + dz * dz;
					if (best < 0 || distance < best) {
						best = distance;
						nearest = i;
					}
				}
				job = std::move(jobs[nearest].run);
				jobs.erase(jobs.begin() + static_cast<std::ptrdiff_t>(nearest));
			}
			job();
		}
	}

	void VoxelWorld::enqueue(int x, int z, bool terrain, std::function<void()> job) {
		{
			std::lock_guard<std::mutex> lock(jobMutex);
			jobs.push_back(Job{ x, z, terrain, std::move(job) });
		}
		jobReady.notify_one();
	}

	void VoxelWorld::setJobFocus(int x, int z) {
		std::lock_guard<std::mutex> lock(jobMutex);
		jobFocusX = x;
		jobFocusZ = z;
	}

	void VoxelWorld::setColour(int blockId, glm::vec3 colour) {
		palette[static_cast<size_t>(std::clamp(blockId, 0, 255))] = colour;
	}

	void VoxelWorld::setSmoothing(int resolution) {
		const int next = std::max(0, resolution);
		if (next == smoothResolution) return;
		smoothResolution = next;
		refreshResolutions(focusCx, focusCz, true);
	}

	void VoxelWorld::setDetail(int nearChunks, int nearSamples) {
		nearRadius = std::max(0, nearChunks);
		nearResolution = std::max(1, nearSamples);
		refreshResolutions(focusCx, focusCz, true);
	}

	void VoxelWorld::setFar(int farChunks, int stride) {
		farRadius = std::max(0, farChunks);
		farStride = std::max(1, stride);
		refreshResolutions(focusCx, focusCz, true);
	}

	int VoxelWorld::strideFor(int cx, int cz) const {
		if (farRadius > 0 && focusKnown && std::max(std::abs(cx - focusCx), std::abs(cz - focusCz)) > farRadius) return farStride;
		return 1;
	}

	int VoxelWorld::resolutionFor(int cx, int cz) const {
		if (smoothResolution <= 0) return 0;
		if (nearRadius > 0 && focusKnown && std::max(std::abs(cx - focusCx), std::abs(cz - focusCz)) <= nearRadius) {
			return std::max(smoothResolution, nearResolution);
		}
		return smoothResolution;
	}

	void VoxelWorld::refreshResolutions(int focusX, int focusZ, bool force) {
		if (!force && focusKnown && focusX == focusCx && focusZ == focusCz) return;
		focusCx = focusX;
		focusCz = focusZ;
		focusKnown = true;
		for (auto& entry : chunks) {
			Chunk& chunk = entry.second;
			if (!chunk.loaded) continue;   // a chunk gets its level when its terrain is installed
			const int wanted = resolutionFor(entry.first.first, entry.first.second);
			const int wantedStride = strideFor(entry.first.first, entry.first.second);
			if (wanted != chunk.resolution || wantedStride != chunk.stride) {
				chunk.resolution = wanted;
				chunk.stride = wantedStride;
				markDirty(entry.first.first, entry.first.second);
				// The neighbours' seams with this chunk change, so they are meshed again too.
				markNeighboursDirty(entry.first.first, entry.first.second);
			}
		}
	}

	int VoxelWorld::chunkQuads(int cx, int cz) const {
		const Chunk* chunk = findChunk(cx, cz);
		return chunk && chunk->loaded ? chunk->quads : 0;
	}

	std::map<int, int> VoxelWorld::chunkGroupQuads(int cx, int cz) const {
		const Chunk* chunk = findChunk(cx, cz);
		return chunk && chunk->loaded ? chunk->groupQuads : std::map<int, int>();
	}

	int VoxelWorld::floorDiv(int a, int b) {
		int q = a / b;
		if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
		return q;
	}

	const VoxelWorld::Chunk* VoxelWorld::findChunk(int cx, int cz) const {
		if (memoChunk && memoX == cx && memoZ == cz) return memoChunk;
		auto it = chunks.find(std::make_pair(cx, cz));
		if (it == chunks.end()) return nullptr;
		memoX = cx;
		memoZ = cz;
		memoChunk = &it->second;
		return memoChunk;
	}

	VoxelWorld::Chunk* VoxelWorld::findChunk(int cx, int cz) {
		auto it = chunks.find(std::make_pair(cx, cz));
		return it == chunks.end() ? nullptr : &it->second;
	}

	int VoxelWorld::getBlock(int x, int y, int z) const {
		if (y < 0 || y >= height) return 0;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		const Chunk* chunk = findChunk(cx, cz);
		if (!chunk || !chunk->loaded) return 0;
		return chunk->blocks[localIndex(x - cx * size, y, z - cz * size)];
	}

	// A write that is not a break (a fill, a liquid, an edit file): it does not look for detached pieces around it.
	void VoxelWorld::setBlockQuiet(int x, int y, int z, int blockId) {
		const bool was = detachChecks;
		detachChecks = false;
		setBlock(x, y, z, blockId);
		detachChecks = was;
	}

	void VoxelWorld::setBlock(int x, int y, int z, int blockId) {
		if (y < 0 || y >= height) return;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		const int lx = x - cx * size;
		const int lz = z - cz * size;
		const uint8_t block = static_cast<uint8_t>(std::clamp(blockId, 0, 255));
		clearDamage(x, y, z);
		// The edit is recorded even if the chunk is not loaded, so it is in place when the chunk loads.
		edits[std::make_pair(cx, cz)][localIndex(lx, y, lz)] = block;
		noteChange(x, y, z);
		Chunk* chunk = findChunk(cx, cz);
		if (!chunk || !chunk->loaded) return;
		chunk->blocks[localIndex(lx, y, lz)] = block;
		// A block near a chunk border is seen from the neighbour too (its skin and its light reach that far), and near a
		// corner from the diagonal chunks.
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dz = -1; dz <= 1; ++dz) {
				if (dx == -1 && lx >= VoxelLightReach) continue;
				if (dx == 1 && lx < size - VoxelLightReach) continue;
				if (dz == -1 && lz >= VoxelLightReach) continue;
				if (dz == 1 && lz < size - VoxelLightReach) continue;
				markDirty(cx + dx, cz + dz);
			}
		}
		// The simulation looks at the block and its neighbours next tick (a falling block, the water beside it, a rule).
		simulation.activate(x, y, z);
		simulation.activate(x + 1, y, z);
		simulation.activate(x - 1, y, z);
		simulation.activate(x, y + 1, z);
		simulation.activate(x, y - 1, z);
		simulation.activate(x, y, z + 1);
		simulation.activate(x, y, z - 1);
		if (block == 0 && detachChecks) detachAround(x, y, z);
	}

	bool VoxelWorld::setProp(int x, int y, int z, int dir) {
		if (y < 0 || y >= height || dir < 0 || dir > 5 || !loadedAt(x, z) || getBlock(x, y, z) != 0) return false;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		const int lx = x - cx * size;
		const int lz = z - cz * size;
		propsByChunk[std::make_pair(cx, cz)][localIndex(lx, y, lz)] = static_cast<uint8_t>(dir);
		markLight(cx, cz, lx, lz);
		noteChange(x, y, z);
		return true;
	}

	void VoxelWorld::removeProp(int x, int y, int z) {
		if (y < 0 || y >= height) return;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		auto chunk = propsByChunk.find(std::make_pair(cx, cz));
		if (chunk == propsByChunk.end()) return;
		const int lx = x - cx * size;
		const int lz = z - cz * size;
		if (chunk->second.erase(localIndex(lx, y, lz)) == 0) return;
		if (chunk->second.empty()) propsByChunk.erase(chunk);
		markLight(cx, cz, lx, lz);
		noteChange(x, y, z);
	}

	int VoxelWorld::propAt(int x, int y, int z) const {
		if (y < 0 || y >= height) return -1;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		auto chunk = propsByChunk.find(std::make_pair(cx, cz));
		if (chunk == propsByChunk.end()) return -1;
		auto entry = chunk->second.find(localIndex(x - cx * size, y, z - cz * size));
		return entry == chunk->second.end() ? -1 : entry->second;
	}

	std::pair<int, std::vector<std::array<int, 3>>> VoxelWorld::drainVolume(int x, int y, int z, int want, bool commit) {
		// Breadth-first over the volume cells of the liquid, from the cell hit, so the nearest units go first. A cell holds up to 8 units.
		struct Cell {
			int x, y, z, units;
		};
		std::pair<int, std::vector<std::array<int, 3>>> result{ 0, {} };
		const VolumeLiquid* family = volumeOf(static_cast<uint8_t>(getBlock(x, y, z)));
		if (!family) return result;
		const size_t MaxCells = 64;   // a bucket reaches this many cells of volume, which is more than any puddle a player makes
		std::vector<Cell> cells;
		std::vector<Cell> taken;
		auto visited = [&cells](int cx, int cy, int cz) {
			for (const Cell& c : cells) {
				if (c.x == cx && c.y == cy && c.z == cz) return true;
			}
			return false;
		};
		cells.push_back({ x, y, z, family->units(static_cast<uint8_t>(getBlock(x, y, z))) });
		const int Neighbours[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for (size_t head = 0; head < cells.size() && result.first < want; ++head) {
			const Cell cell = cells[head];
			if (cell.units > 0) {
				const int units = std::min(cell.units, want - result.first);
				result.first += units;
				taken.push_back({ cell.x, cell.y, cell.z, cell.units - units });
			}
			for (const auto& d : Neighbours) {
				const int nx = cell.x + d[0], ny = cell.y + d[1], nz = cell.z + d[2];
				if (cells.size() >= MaxCells || visited(nx, ny, nz)) continue;
				const int units = family->units(static_cast<uint8_t>(getBlock(nx, ny, nz)));
				if (units > 0) cells.push_back({ nx, ny, nz, units });
			}
		}
		for (const Cell& cell : taken) {
			result.second.push_back({ cell.x, cell.y, cell.z });
			if (commit) setBlockQuiet(cell.x, cell.y, cell.z, cell.units == 0 ? 0 : family->block(cell.units));
		}
		return result;
	}

	std::pair<int, std::vector<std::array<int, 3>>> VoxelWorld::pourVolume(int x, int y, int z, int units, int unitOne, bool commit) {
		// Breadth-first from the cell poured into, over air and the volume cells of the liquid that have room: the nearest cells fill first.
		struct Cell {
			int x, y, z, units;
		};
		std::pair<int, std::vector<std::array<int, 3>>> result{ 0, {} };
		if (unitOne < 0 || unitOne > 255) return result;
		const VolumeLiquid* family = volumeOf(static_cast<uint8_t>(unitOne));
		if (!family || units <= 0) return result;
		const size_t MaxCells = 64;
		// The cells the liquid can go into: air, or the family's cells with room, as (x, y, z, units they hold now).
		auto holds = [&](int cx, int cy, int cz) -> int {
			const int block = getBlock(cx, cy, cz);
			if (block == 0) return 0;
			const int here = family->units(static_cast<uint8_t>(block));
			return here > 0 ? here : -1;   // -1: another block, which takes no liquid
		};
		std::vector<Cell> cells;
		std::vector<Cell> filled;
		auto visited = [&cells](int cx, int cy, int cz) {
			for (const Cell& c : cells) {
				if (c.x == cx && c.y == cy && c.z == cz) return true;
			}
			return false;
		};
		const int start = holds(x, y, z);
		if (start < 0) return result;
		cells.push_back({ x, y, z, start });
		const int Neighbours[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for (size_t head = 0; head < cells.size() && result.first < units; ++head) {
			const Cell cell = cells[head];
			const int room = family->Full - cell.units;
			if (room > 0) {
				const int put = std::min(room, units - result.first);
				result.first += put;
				filled.push_back({ cell.x, cell.y, cell.z, cell.units + put });
			}
			for (const auto& d : Neighbours) {
				const int nx = cell.x + d[0], ny = cell.y + d[1], nz = cell.z + d[2];
				if (cells.size() >= MaxCells || visited(nx, ny, nz)) continue;
				const int here = holds(nx, ny, nz);
				if (here >= 0 && here < family->Full) cells.push_back({ nx, ny, nz, here });
			}
		}
		for (const Cell& cell : filled) {
			result.second.push_back({ cell.x, cell.y, cell.z });
			if (commit) setBlockQuiet(cell.x, cell.y, cell.z, family->block(cell.units));
		}
		return result;
	}

	int VoxelWorld::hitBlock(int x, int y, int z) {
		const int block = getBlock(x, y, z);
		if (block == 0) return -2;
		const int hits = std::max(1, static_cast<int>(types[static_cast<size_t>(block)].hits));
		const int stage = damageAt(x, y, z) + 1;
		if (stage >= hits) {
			setBlock(x, y, z, 0);   // setBlock also clears the block's damage
			return -1;
		}
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		damageByChunk[std::make_pair(cx, cz)][localIndex(x - cx * size, y, z - cz * size)] = static_cast<uint8_t>(stage);
		markLight(cx, cz, x - cx * size, z - cz * size);   // the stage shows in this chunk's mesh, and in the neighbours that draw its skin
		noteChange(x, y, z);
		return stage;
	}

	int VoxelWorld::healBlock(int x, int y, int z) {
		const int stage = damageAt(x, y, z);
		if (stage <= 1) {
			clearDamage(x, y, z);
			return 0;
		}
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		damageByChunk[std::make_pair(cx, cz)][localIndex(x - cx * size, y, z - cz * size)] = static_cast<uint8_t>(stage - 1);
		markLight(cx, cz, x - cx * size, z - cz * size);
		noteChange(x, y, z);
		return stage - 1;
	}

	int VoxelWorld::damageAt(int x, int y, int z) const {
		if (y < 0 || y >= height) return 0;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		auto chunk = damageByChunk.find(std::make_pair(cx, cz));
		if (chunk == damageByChunk.end()) return 0;
		auto entry = chunk->second.find(localIndex(x - cx * size, y, z - cz * size));
		return entry == chunk->second.end() ? 0 : entry->second;
	}

	void VoxelWorld::clearDamage(int x, int y, int z) {
		if (y < 0 || y >= height) return;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		auto chunk = damageByChunk.find(std::make_pair(cx, cz));
		if (chunk == damageByChunk.end()) return;
		if (chunk->second.erase(localIndex(x - cx * size, y, z - cz * size)) == 0) return;
		if (chunk->second.empty()) damageByChunk.erase(chunk);
		markLight(cx, cz, x - cx * size, z - cz * size);
		noteChange(x, y, z);
	}

	void VoxelWorld::setDamage(int x, int y, int z, int stage) {
		if (stage <= 0) {
			clearDamage(x, y, z);
			return;
		}
		if (y < 0 || y >= height) return;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		damageByChunk[std::make_pair(cx, cz)][localIndex(x - cx * size, y, z - cz * size)] = static_cast<uint8_t>(stage);
		markLight(cx, cz, x - cx * size, z - cz * size);
		noteChange(x, y, z);
	}

	void VoxelWorld::setRecordChanges(bool on) {
		recordChanges = on;
		if (!on) changedCells.clear();
	}

	std::vector<std::array<int, 3>> VoxelWorld::takeChanges() {
		std::vector<std::array<int, 3>> out(changedCells.begin(), changedCells.end());
		changedCells.clear();
		return out;
	}

	std::vector<std::array<int, 3>> VoxelWorld::editedCells() const {
		std::set<std::array<int, 3>> cells;
		// A cell's key in a chunk is localIndex: (y * size + lz) * size + lx, so x, y and z come back out of it.
		auto add = [&](const std::pair<int, int>& chunk, int local) {
			const int lx = local % size;
			const int lz = (local / size) % size;
			const int y = local / (size * size);
			cells.insert(std::array<int, 3>{ chunk.first * size + lx, y, chunk.second * size + lz });
		};
		for (const auto& chunk : edits) for (const auto& entry : chunk.second) add(chunk.first, entry.first);
		for (const auto& chunk : damageByChunk) for (const auto& entry : chunk.second) add(chunk.first, entry.first);
		for (const auto& chunk : propsByChunk) for (const auto& entry : chunk.second) add(chunk.first, entry.first);
		return std::vector<std::array<int, 3>>(cells.begin(), cells.end());
	}

	float VoxelWorld::propAnchor(int x, int y, int z, int dir) const {
		// March from the centre of the block the prop leans on toward the prop's cell, to where the skin stops being inside.
		// The skin is the one collision uses, which is the drawn surface.
		const double wx = x + 0.5 + PropDirs[dir][0];
		const double wy = y + 0.5 + PropDirs[dir][1];
		const double wz = z + 0.5 + PropDirs[dir][2];
		if (skinDensity(wx, wy, wz) <= VoxelSkinIso) return 0.5f;
		const double cx = x + 0.5;
		const double cy = y + 0.5;
		const double cz = z + 0.5;
		for (int i = 1; i <= 100; ++i) {
			const double t = i / 100.0;   // 0 at the block's centre, 1 at the prop's cell centre
			const double px = wx + (cx - wx) * t;
			const double py = wy + (cy - wy) * t;
			const double pz = wz + (cz - wz) * t;
			if (skinDensity(px, py, pz) <= VoxelSkinIso) return static_cast<float>(1.0 - t);
		}
		return 0.5f;
	}

	std::vector<std::array<int, 4>> VoxelWorld::props() const {
		std::vector<std::array<int, 4>> list;
		for (const auto& chunk : propsByChunk) {
			for (const auto& entry : chunk.second) {
				const int lx = entry.first % size;
				const int lz = (entry.first / size) % size;
				const int y = entry.first / (size * size);
				list.push_back({ chunk.first.first * size + lx, y, chunk.first.second * size + lz, entry.second });
			}
		}
		return list;
	}

	void VoxelWorld::markLight(int cx, int cz, int lx, int lz) {
		// The same neighbours as setBlock: a cell's light reaches VoxelLightReach blocks, so the chunks it is that close to.
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dz = -1; dz <= 1; ++dz) {
				if (dx == -1 && lx >= VoxelLightReach) continue;
				if (dx == 1 && lx < size - VoxelLightReach) continue;
				if (dz == -1 && lz >= VoxelLightReach) continue;
				if (dz == 1 && lz < size - VoxelLightReach) continue;
				markDirty(cx + dx, cz + dz);
			}
		}
	}

	void VoxelWorld::fillColumn(int x, int z, int yFrom, int yTo, int blockId) {
		for (int y = std::max(0, yFrom); y < std::min(height, yTo); ++y) {
			setBlockQuiet(x, y, z, blockId);
		}
	}

		void VoxelWorld::markDirty(int cx, int cz) {
			Chunk* chunk = findChunk(cx, cz);
			if (!chunk) return;
			chunk->counts.clear();   // the terrain's corner counts are out of date (terrainCorners)
			chunk->dirty = true;
			markRegionDirty(regionOf(cx, cz));
		}

	std::pair<int, int> VoxelWorld::regionOf(int cx, int cz) {
		return { floorDiv(cx, RegionChunks), floorDiv(cz, RegionChunks) };
	}

		void VoxelWorld::markRegionDirty(std::pair<int, int> key) {
			Region& region = regions[key];
			region.settle = 0;
			dirtyRegions.insert(key);
		}

	bool VoxelWorld::neighboursSettled(int cx, int cz) const {
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dx = -1; dx <= 1; ++dx) {
				if (dx == 0 && dz == 0) continue;
				const Chunk* chunk = findChunk(cx + dx, cz + dz);
				if (chunk && !chunk->loaded) return false;   // requested, its terrain still on its way
			}
		}
		return true;
	}

	int VoxelWorld::loadedChunksIn(std::pair<int, int> key) const {
		int count = 0;
		for (int lz = 0; lz < RegionChunks; ++lz) {
			for (int lx = 0; lx < RegionChunks; ++lx) {
				const Chunk* chunk = findChunk(key.first * RegionChunks + lx, key.second * RegionChunks + lz);
				if (chunk && chunk->loaded) ++count;
			}
		}
		return count;
	}

	void VoxelWorld::removeRegionBodies(std::pair<int, int> key) {
		auto it = regions.find(key);
		if (it == regions.end()) return;
		Region& region = it->second;
		for (const auto& body : region.bodies) {
			if (Body* mesh = assets.get<Body>(body.second)) removeRuntimeMesh(mesh);
		}
		region.bodies.clear();
		++region.epoch;   // a mesh still on a worker is for the old bodies, and is dropped when it arrives
		dirtyRegions.erase(key);
	}

	VoxelBodyState VoxelWorld::stepBody(const VoxelBodyState& body, const VoxelBodyShape& shape, double dt, bool flying) const {
		return stepVoxelBody(*this, shape, body, dt, flying);
	}

	void VoxelWorld::markNeighboursDirty(int cx, int cz) {
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dz = -1; dz <= 1; ++dz) {
				if (dx != 0 || dz != 0) markDirty(cx + dx, cz + dz);
			}
		}
	}

	bool VoxelWorld::requestChunk(int cx, int cz) {
		const auto key = std::make_pair(cx, cz);
		if (chunks.count(key)) return false;
		Chunk& chunk = chunks[key];
		chunk.generation = nextGeneration++;
		const uint32_t generation = chunk.generation;
		const VoxelTerrain settings = terrain;
		const int chunkSize = size;
		const int chunkHeight = height;
		enqueue(cx, cz, true, [this, settings, cx, cz, generation, chunkSize, chunkHeight] {
			const auto began = std::chrono::steady_clock::now();
			TerrainResult result;
			result.cx = cx;
			result.cz = cz;
			result.generation = generation;
			settings.fillChunk(cx, cz, chunkSize, chunkHeight, result.blocks);
			result.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
			std::lock_guard<std::mutex> lock(resultMutex);
			terrainResults.push_back(std::move(result));
		});
		return true;
	}

	bool VoxelWorld::isLoaded(int cx, int cz) const {
		const Chunk* chunk = findChunk(cx, cz);
		return chunk && chunk->loaded;
	}

	void VoxelWorld::unloadChunk(int cx, int cz) {
		auto it = chunks.find(std::make_pair(cx, cz));
		if (it == chunks.end()) return;
		chunks.erase(it);
		{
			// Its terrain is not wanted any more: a queued job for it is dropped, not run.
			std::lock_guard<std::mutex> lock(jobMutex);
			jobs.erase(std::remove_if(jobs.begin(), jobs.end(), [cx, cz](const Job& job) {
				return job.terrain && job.x == cx && job.z == cz;
			}), jobs.end());
		}
		memoChunk = nullptr;
		// The region's mesh still has this chunk in it, so the region is meshed again without it (or removed, when it is empty).
		const auto key = regionOf(cx, cz);
		if (loadedChunksIn(key) == 0) {
			removeRegionBodies(key);
		} else {
			markRegionDirty(key);
		}
		markNeighboursDirty(cx, cz);
	}

	void VoxelWorld::installTerrain(TerrainResult& result) {
		auto it = chunks.find(std::make_pair(result.cx, result.cz));
		if (it == chunks.end() || it->second.generation != result.generation || it->second.loaded) return;
		Chunk& chunk = it->second;
		chunk.blocks = std::move(result.blocks);
		applyEdits(result.cx, result.cz, chunk.blocks);
		chunk.loaded = true;
		chunk.resolution = resolutionFor(result.cx, result.cz);
		chunk.stride = strideFor(result.cx, result.cz);
		// The chunk's faces, and those of the neighbours that face it, are meshed from the new blocks.
		markDirty(result.cx, result.cz);
		markNeighboursDirty(result.cx, result.cz);
	}

	void VoxelWorld::applyEdits(int cx, int cz, std::vector<uint8_t>& blocks) const {
		auto it = edits.find(std::make_pair(cx, cz));
		if (it == edits.end()) return;
		for (const auto& edit : it->second) blocks[static_cast<size_t>(edit.first)] = edit.second;
	}

	int VoxelWorld::editCount() const {
		int count = 0;
		for (const auto& chunkEdits : edits) count += static_cast<int>(chunkEdits.second.size());
		return count;
	}

	bool VoxelWorld::saveEdits(const std::string& path) const {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out) return false;
		const auto put32 = [&out](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
		out.write("CGVE", 4);
		put32(3);   // version 3: the blocks, then the torches, then the damage
		put32(static_cast<uint32_t>(editCount()));
		for (const auto& chunkEdits : edits) {
			const int cx = chunkEdits.first.first;
			const int cz = chunkEdits.first.second;
			for (const auto& edit : chunkEdits.second) {
				const int y = edit.first / (size * size);
				const int lz = (edit.first % (size * size)) / size;
				const int lx = edit.first % size;
				put32(static_cast<uint32_t>(cx * size + lx));
				put32(static_cast<uint32_t>(y));
				put32(static_cast<uint32_t>(cz * size + lz));
				out.put(static_cast<char>(edit.second));
			}
		}
		// The torches, after the blocks: a count, then x, y, z (int32) and dir (a byte) for each.
		uint32_t torches = 0;
		for (const auto& chunkProps : propsByChunk) torches += static_cast<uint32_t>(chunkProps.second.size());
		put32(torches);
		for (const auto& chunkProps : propsByChunk) {
			const int cx = chunkProps.first.first;
			const int cz = chunkProps.first.second;
			for (const auto& entry : chunkProps.second) {
				const int y = entry.first / (size * size);
				const int lz = (entry.first % (size * size)) / size;
				const int lx = entry.first % size;
				put32(static_cast<uint32_t>(cx * size + lx));
				put32(static_cast<uint32_t>(y));
				put32(static_cast<uint32_t>(cz * size + lz));
				out.put(static_cast<char>(entry.second));
			}
		}
		// The damage, after the torches: a count, then x, y, z (int32) and the stage (a byte) for each damaged block.
		uint32_t damaged = 0;
		for (const auto& chunkDamage : damageByChunk) damaged += static_cast<uint32_t>(chunkDamage.second.size());
		put32(damaged);
		for (const auto& chunkDamage : damageByChunk) {
			const int cx = chunkDamage.first.first;
			const int cz = chunkDamage.first.second;
			for (const auto& entry : chunkDamage.second) {
				const int y = entry.first / (size * size);
				const int lz = (entry.first % (size * size)) / size;
				const int lx = entry.first % size;
				put32(static_cast<uint32_t>(cx * size + lx));
				put32(static_cast<uint32_t>(y));
				put32(static_cast<uint32_t>(cz * size + lz));
				out.put(static_cast<char>(entry.second));
			}
		}
		return static_cast<bool>(out);
	}

	bool VoxelWorld::loadEdits(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		const auto get32 = [&in](uint32_t& v) { in.read(reinterpret_cast<char*>(&v), 4); return static_cast<bool>(in); };
		char magic[4] = {};
		uint32_t version = 0;
		uint32_t count = 0;
		in.read(magic, 4);
		if (!in || std::string(magic, 4) != "CGVE" || !get32(version) || (version < 1 || version > 3) || !get32(count)) return false;
		for (uint32_t i = 0; i < count; ++i) {
			uint32_t x = 0, y = 0, z = 0;
			if (!get32(x) || !get32(y) || !get32(z)) return false;
			const int block = in.get();
			if (block == std::char_traits<char>::eof()) return false;
			const int wx = static_cast<int>(x);
			const int wy = static_cast<int>(y);
			const int wz = static_cast<int>(z);
			if (wy < 0 || wy >= height) continue;
			const int cx = floorDiv(wx, size);
			const int cz = floorDiv(wz, size);
			edits[std::make_pair(cx, cz)][localIndex(wx - cx * size, wy, wz - cz * size)] = static_cast<uint8_t>(block);
		}
		// Version 2 carries the torches after the blocks (see saveEdits). They are kept like the edits, by chunk.
		if (version >= 2) {
			uint32_t torches = 0;
			if (!get32(torches)) return false;
			for (uint32_t i = 0; i < torches; ++i) {
				uint32_t x = 0, y = 0, z = 0;
				if (!get32(x) || !get32(y) || !get32(z)) return false;
				const int dir = in.get();
				if (dir == std::char_traits<char>::eof()) return false;
				const int wx = static_cast<int>(x);
				const int wy = static_cast<int>(y);
				const int wz = static_cast<int>(z);
				if (wy < 0 || wy >= height || dir > 5) continue;
				const int cx = floorDiv(wx, size);
				const int cz = floorDiv(wz, size);
				propsByChunk[std::make_pair(cx, cz)][localIndex(wx - cx * size, wy, wz - cz * size)] = static_cast<uint8_t>(dir);
			}
			for (const auto& chunkProps : propsByChunk) {
				if (isLoaded(chunkProps.first.first, chunkProps.first.second)) markNeighboursDirty(chunkProps.first.first, chunkProps.first.second);
				if (isLoaded(chunkProps.first.first, chunkProps.first.second)) markDirty(chunkProps.first.first, chunkProps.first.second);
			}
		}

		// Version 3 carries the damage after the torches: the stage of each damaged block, kept by chunk like the torches.
		if (version >= 3) {
			uint32_t damaged = 0;
			if (!get32(damaged)) return false;
			for (uint32_t i = 0; i < damaged; ++i) {
				uint32_t x = 0, y = 0, z = 0;
				if (!get32(x) || !get32(y) || !get32(z)) return false;
				const int stage = in.get();
				if (stage == std::char_traits<char>::eof()) return false;
				const int wx = static_cast<int>(x);
				const int wy = static_cast<int>(y);
				const int wz = static_cast<int>(z);
				if (wy < 0 || wy >= height || stage <= 0) continue;
				const int cx = floorDiv(wx, size);
				const int cz = floorDiv(wz, size);
				damageByChunk[std::make_pair(cx, cz)][localIndex(wx - cx * size, wy, wz - cz * size)] = static_cast<uint8_t>(stage);
			}
			for (const auto& chunkDamage : damageByChunk) {
				if (isLoaded(chunkDamage.first.first, chunkDamage.first.second)) markNeighboursDirty(chunkDamage.first.first, chunkDamage.first.second);
			}
		}

		// Loaded chunks take the edits now; the rest take them when they load.
		for (const auto& chunkEdits : edits) {
			Chunk* chunk = findChunk(chunkEdits.first.first, chunkEdits.first.second);
			if (!chunk || !chunk->loaded) continue;
			applyEdits(chunkEdits.first.first, chunkEdits.first.second, chunk->blocks);
			markDirty(chunkEdits.first.first, chunkEdits.first.second);
			markNeighboursDirty(chunkEdits.first.first, chunkEdits.first.second);
		}
		return true;
	}

	PaddedChunk VoxelWorld::snapshot(int cx, int cz, int border) const {
		PaddedChunk padded;
		padded.size = size;
		padded.height = height;
		padded.border = border;
		const int p = size + 2 * border;
		padded.blocks.assign(static_cast<size_t>(p) * p * (height + 2 * border), 0);

		// The nine chunks around (cx, cz), indexed by the offset of the chunk from it: [dz + 1][dx + 1].
		const Chunk* around[3][3];
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dx = -1; dx <= 1; ++dx) {
				const Chunk* chunk = findChunk(cx + dx, cz + dz);
				around[dz + 1][dx + 1] = chunk && chunk->loaded ? chunk : nullptr;
			}
		}
		// Copied a run at a time: a row along x stays in one neighbour except where it crosses a chunk border, so each row
		// takes at most three copies. This runs on the world's thread for every chunk it meshes, so it has to be quick.
		for (int y = 0; y < height; ++y) {
			const int py = y + border;
			for (int pz = 0; pz < p; ++pz) {
				const int lz = pz - border;
				const int nz = lz < 0 ? -1 : (lz >= size ? 1 : 0);
				const int inZ = lz - nz * size;
				for (int px = 0; px < p;) {
					const int lx = px - border;
					const int nx = lx < 0 ? -1 : (lx >= size ? 1 : 0);
					const int inX = lx - nx * size;
					const int run = nx < 0 ? -lx : (nx == 0 ? size - lx : p - px);
					const Chunk* source = around[nz + 1][nx + 1];
					if (source) {
						std::memcpy(&padded.blocks[static_cast<size_t>((py * p + pz) * p + px)],
							&source->blocks[static_cast<size_t>(localIndex(inX, y, inZ))], static_cast<size_t>(run));
					}
					px += run;
				}
			}
		}
		// The torches of the chunk and its neighbours that reach into the padded area (PaddedChunk::props).
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dx = -1; dx <= 1; ++dx) {
				auto chunk = propsByChunk.find(std::make_pair(cx + dx, cz + dz));
				if (chunk == propsByChunk.end()) continue;
				for (const auto& entry : chunk->second) {
					const int px = dx * size + entry.first % size;
					const int pz = dz * size + (entry.first / size) % size;
					const int py = entry.first / (size * size);
					if (px < -border || px >= size + border || pz < -border || pz >= size + border) continue;
					const float anchor = propAnchor(cx * size + px, py, cz * size + pz, entry.second);
					padded.props.push_back({ px, py, pz, entry.second, anchor, propNormal(cx * size + px, py, cz * size + pz, entry.second) });
				}
			}
		}
		// The damage of the blocks in the padded area, as the fraction of their hits taken (PaddedChunk::damage).
		bool anyDamage = false;
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dx = -1; dx <= 1; ++dx) anyDamage = anyDamage || damageByChunk.count(std::make_pair(cx + dx, cz + dz));
		}
		if (anyDamage) {
			padded.damage.assign(padded.blocks.size(), 0);
			for (int dz = -1; dz <= 1; ++dz) {
				for (int dx = -1; dx <= 1; ++dx) {
					auto chunk = damageByChunk.find(std::make_pair(cx + dx, cz + dz));
					if (chunk == damageByChunk.end()) continue;
					for (const auto& entry : chunk->second) {
						const int px = dx * size + entry.first % size;
						const int pz = dz * size + (entry.first / size) % size;
						const int py = entry.first / (size * size);
						if (px < -border || px >= size + border || pz < -border || pz >= size + border) continue;
						const int hits = std::max(1, static_cast<int>(types[static_cast<size_t>(getBlock(cx * size + px, py, cz * size + pz))].hits));
						padded.damage[padded.index(px, py, pz)] = static_cast<uint8_t>(std::min(255, entry.second * 255 / hits));
					}
				}
			}
		}
		for (size_t id = 0; id < 256; ++id) {
			padded.emission[id] = types[id].emits;
			padded.rule[id].group = meshGroup(types[id], static_cast<uint8_t>(id));
			padded.rule[id].join = types[id].joinAbove;
			padded.rule[id].joinBelow = types[id].joinBelow;
			padded.rule[id].cube = types[id].cube;
			padded.rule[id].blend = types[id].blend;
			padded.rule[id].passesLight = types[id].passesLight;
			padded.rule[id].textured = types[id].textured;
			padded.rule[id].splat = types[id].splat;
			for (int k = 0; k < 3; ++k) padded.rule[id].tiles[k] = types[id].tiles[k];
		}
		return padded;
	}

		void VoxelWorld::startMeshing(std::pair<int, int> key) {
			Region& region = regions[key];
			region.meshing = true;
			dirtyRegions.erase(key);
			// Each loaded chunk of the region is copied here, on this thread, with its neighbours, so the worker sees a fixed copy.
			// A chunk that has not changed keeps the mesh it was built to, so it is not copied or meshed again.
			std::vector<ChunkInput> inputs;
			for (int lz = 0; lz < RegionChunks; ++lz) {
				for (int lx = 0; lx < RegionChunks; ++lx) {
					const int cx = key.first * RegionChunks + lx;
					const int cz = key.second * RegionChunks + lz;
					Chunk* chunk = findChunk(cx, cz);
					if (!chunk || !chunk->loaded) continue;
					ChunkInput input;
					input.cx = cx;
					input.cz = cz;
					input.generation = chunk->generation;
					input.resolution = chunk->resolution;
					input.stride = chunk->stride;
					// Skirts on the sides that meet a neighbour at another level of detail (bit 1: -x, 2: +x, 4: -z, 8: +z).
					if (input.resolution > 0) {
						const int dxs[4] = { -1, 1, 0, 0 };
						const int dzs[4] = { 0, 0, -1, 1 };
						for (int side = 0; side < 4; ++side) {
							const Chunk* neighbour = findChunk(cx + dxs[side], cz + dzs[side]);
							if (neighbour && neighbour->loaded && (neighbour->resolution != input.resolution || neighbour->stride != input.stride)) {
								input.skirtSides |= 1u << side;
							}
						}
					}
					if (chunk->dirty || !chunk->mesh) {
						input.padded = snapshot(cx, cz, VoxelLightReach);
					} else {
						input.reuse = chunk->mesh;
					}
					chunk->dirty = false;
					inputs.push_back(std::move(input));
				}
			}
			const std::array<glm::vec3, 256> colours = palette;
			std::array<bool, 256> cubeShape{};
			for (size_t id = 0; id < 256; ++id) cubeShape[id] = types[id].cube;
			const uint32_t epoch = region.epoch;
			enqueue(key.first * RegionChunks + RegionChunks / 2, key.second * RegionChunks + RegionChunks / 2, false,
				[this, key, epoch, inputs = std::move(inputs), colours, cubeShape]() mutable {
				const auto began = std::chrono::steady_clock::now();
				RegionResult result;
				result.rx = key.first;
				result.rz = key.second;
				result.epoch = epoch;
				std::map<int, MeshArrays> merged;
				std::map<int, int> mergedQuads;
				const auto stage = [](std::chrono::steady_clock::time_point since) {
					return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - since).count();
				};
				for (ChunkInput& input : inputs) {
					std::shared_ptr<const ChunkMesh> mesh = input.reuse;
					if (!mesh) {
						auto clock = std::chrono::steady_clock::now();
						computeLight(input.padded);
						result.lightMs += stage(clock);
						clock = std::chrono::steady_clock::now();
						if (input.resolution > 0) computeSkin(input.padded);
						result.skinMs += stage(clock);
						clock = std::chrono::steady_clock::now();
						auto built = std::make_shared<ChunkMesh>();
						// The terrain, and a mesh of its own for each block type in the chunk that has one (see PaddedChunk::rule).
						// The groups: the terrain, and each block type with blocks in the chunk. A smooth mesh takes the groups' bounds, which
						// need the skin (computeSkin); a chunk of cubes has no skin, so it finds its groups from the rules.
						std::array<GroupBounds, 256> bounds{};
						std::array<bool, 256> present{};
						if (input.resolution > 0) {
							bounds = groupBounds(input.padded);
							for (size_t id = 1; id < 256; ++id) present[id] = bounds[id].lo[0] <= bounds[id].hi[0];
						} else {
							for (uint8_t block : input.padded.blocks) {
								if (input.padded.rule[block].group != 0) present[input.padded.rule[block].group] = true;
							}
						}
						std::vector<uint8_t> groups = { 0 };
						for (size_t id = 1; id < 256; ++id) {
							if (present[id]) groups.push_back(static_cast<uint8_t>(id));
						}
						for (uint8_t group : groups) {
							MeshArrays& part = built->groups[group];
							// A cube block is always cubes; the others are cubes only while the world draws cubes (smoothing 0).
							const bool cubes = input.resolution <= 0 || (group != 0 && cubeShape[group]);
							if (cubes) {
								buildChunkMesh(input.padded, colours, group, part);
							} else {
								buildSmoothMesh(input.padded, colours, input.resolution, input.stride, input.skirtSides, group, bounds[group], part);
							}
							// The torches are drawn with the terrain (their cells are air to it, so they are not part of any block's mesh).
							if (group == 0) buildPropMesh(input.padded, part);
						}
						result.surfaceMs += stage(clock);
						++result.built;
						mesh = built;
					}
					// The chunk's place in the region: its meshes are local to the chunk, and the region's body is at its corner.
					const float offsetX = static_cast<float>((input.cx - key.first * RegionChunks) * size);
					const float offsetZ = static_cast<float>((input.cz - key.second * RegionChunks) * size);
					for (const auto& entry : mesh->groups) {
						appendMesh(merged[entry.first], entry.second, offsetX, offsetZ);
						mergedQuads[entry.first] += static_cast<int>(entry.second.indices.size() / 6);
					}
					result.chunks.push_back({ input.cx, input.cz, input.generation, mesh });
				}
				for (auto& entry : merged) {
					if (entry.second.vertices.empty()) continue;
					GroupMesh group;
					group.group = static_cast<uint8_t>(entry.first);
					group.mesh = std::move(entry.second);
					group.quads = mergedQuads[entry.first];
					result.groups.push_back(std::move(group));
				}
				result.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
				std::lock_guard<std::mutex> lock(resultMutex);
				meshResults.push_back(std::move(result));
			});
		}

	std::string VoxelWorld::bodyName(int rx, int rz, uint8_t group) const {
		std::string name = "voxel_region_" + std::to_string(rx) + "_" + std::to_string(rz);
		if (group != 0) name += "_" + std::to_string(group);
		return name;
	}

		void VoxelWorld::uploadMesh(RegionResult& result) {
			const auto key = std::make_pair(result.rx, result.rz);
			auto it = regions.find(key);
			if (it == regions.end()) return;
			Region& region = it->second;
			region.meshing = false;
			// The region's bodies were removed while it was being meshed, so this mesh is dropped.
			if (result.epoch != region.epoch) return;
		++uploads;
			// Each chunk keeps the mesh it was built to, so the next region job reuses it. A chunk that changed while its mesh was
			// built is dirty again, so its region is meshed again after this upload; until then the body is one change behind.
			for (const ChunkBuilt& built : result.chunks) {
				Chunk* chunk = findChunk(built.cx, built.cz);
				if (!chunk || !chunk->loaded || chunk->generation != built.generation) continue;
				chunk->mesh = built.mesh;
				chunk->quads = 0;
				chunk->groupQuads.clear();
				for (const auto& entry : built.mesh->groups) {
					const int quads = static_cast<int>(entry.second.indices.size() / 6);
					chunk->quads += quads;
					if (!entry.second.vertices.empty()) chunk->groupQuads[entry.first] = quads;
				}
			}

		const Vector3f position(static_cast<float>(key.first * RegionChunks * size), 0.0f,
			static_cast<float>(key.second * RegionChunks * size));
		std::set<int> drawn;
		for (GroupMesh& part : result.groups) {
			drawn.insert(part.group);
			const std::string& materialName = part.group != 0 && !groupMaterials[part.group].empty() ? groupMaterials[part.group] : material;
			std::vector<id_t> materials;
			if (auto materialId = assets.getId<Material>(materialName)) {
				materials.push_back(materialId.value());
			} else {
				materials.push_back(assets.getDefaultId<Material>().value_or(0));
			}
			Body* body = nullptr;
			const auto existing = region.bodies.find(part.group);
			if (existing != region.bodies.end()) body = assets.get<Body>(existing->second);
			if (body) {
				updateCompactMeshBody(body, std::move(part.mesh.vertices), std::move(part.mesh.indices));
			} else {
				body = createCompactMeshBody(bodyName(key.first, key.second, part.group), std::move(part.mesh.vertices),
					std::move(part.mesh.indices), materials, position);
				if (body) region.bodies[part.group] = body->getId().value_or(0);
			}
			if (body) body->transparent = types[part.group].blend;
		}
		// A group that is drawn no more (its blocks were taken away) loses its body.
		for (auto drawnBody = region.bodies.begin(); drawnBody != region.bodies.end();) {
			if (drawn.count(drawnBody->first)) {
				++drawnBody;
				continue;
			}
			if (Body* body = assets.get<Body>(drawnBody->second)) removeRuntimeMesh(body);
			drawnBody = region.bodies.erase(drawnBody);
		}
	}

	int VoxelWorld::update(double budgetMs, int focusX, int focusZ) {
		setJobFocus(focusX, focusZ);
		refreshResolutions(focusX, focusZ, false);
		updateMovingBlocks();
		// 1. Install the terrain that finished generating.
		std::deque<TerrainResult> installed;
		{
			std::lock_guard<std::mutex> lock(resultMutex);
			installed.swap(terrainResults);
		}
		const auto installBegan = std::chrono::steady_clock::now();
		for (TerrainResult& result : installed) {
			totals.terrain += result.ms;
			installTerrain(result);
		}
		totals.install += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - installBegan).count();

		// 2. Upload finished meshes, until the budget is spent. At least one, so the queue always moves.
		const auto start = std::chrono::steady_clock::now();
		int uploaded = 0;
		for (;;) {
			RegionResult result;
			{
				std::lock_guard<std::mutex> lock(resultMutex);
				if (meshResults.empty()) break;
				const double spent = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
				if (uploaded > 0 && spent >= budgetMs) break;
				result = std::move(meshResults.front());
				meshResults.pop_front();
			}
			totals.mesh += result.ms;
			totals.light += result.lightMs;
			totals.builds += result.built;
			totals.skin += result.skinMs;
			totals.surface += result.surfaceMs;
			uploadMesh(result);
			++uploaded;
		}
		uploadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

		// 3. Start meshing the changed regions nearest the focus, once they have settled. A region costs its loaded chunks against
		//    MaxMeshStartsPerUpdate, but the first one always starts, so the queue moves.
		std::vector<std::pair<int, std::pair<int, int>>> candidates;
		const std::vector<std::pair<int, int>> keys(dirtyRegions.begin(), dirtyRegions.end());
		for (const auto& key : keys) {
			if (loadedChunksIn(key) == 0) {
				dirtyRegions.erase(key);
				continue;
			}
			Region& region = regions[key];
			if (region.meshing) continue;
			if (++region.settle < RegionSettleUpdates) continue;
			// A region is meshed when the chunks around its loaded ones are in too, so a chunk is not meshed once for each neighbour.
			bool settled = true;
			for (int lz = 0; lz < RegionChunks && settled; ++lz) {
				for (int lx = 0; lx < RegionChunks && settled; ++lx) {
					const int cx = key.first * RegionChunks + lx, cz = key.second * RegionChunks + lz;
					const Chunk* chunk = findChunk(cx, cz);
					if (chunk && chunk->loaded && !neighboursSettled(cx, cz)) settled = false;
				}
			}
			if (!settled) continue;
			const int dx = key.first * RegionChunks + RegionChunks / 2 - focusX;
			const int dz = key.second * RegionChunks + RegionChunks / 2 - focusZ;
			candidates.push_back({ dx * dx + dz * dz, key });
		}
		std::sort(candidates.begin(), candidates.end());
		const auto startBegan = std::chrono::steady_clock::now();
		int budget = MaxMeshStartsPerUpdate;
		for (size_t i = 0; i < candidates.size(); ++i) {
			const auto key = candidates[i].second;
			const int loaded = loadedChunksIn(key);
			if (i > 0 && loaded > budget) continue;
			startMeshing(key);
			budget -= loaded;
		}
		totals.start += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startBegan).count();
		return uploaded;
	}

	int VoxelWorld::loadedCount() const {
		int count = 0;
		for (const auto& entry : chunks) {
			if (entry.second.loaded) ++count;
		}
		return count;
	}

	int VoxelWorld::readyWithin(int cx, int cz, int radius) const {
		int count = 0;
		for (int z = cz - radius; z <= cz + radius; ++z) {
			for (int x = cx - radius; x <= cx + radius; ++x) {
				const Chunk* chunk = findChunk(x, z);
				if (!chunk || !chunk->loaded || chunk->dirty || !chunk->mesh) continue;
				const auto region = regions.find(regionOf(x, z));
				if (region != regions.end() && region->second.meshing) continue;
				++count;
			}
		}
		return count;
	}

	int VoxelWorld::pendingCount() const {
		int count = 0;
		for (const auto& entry : chunks) {
			if (!entry.second.loaded) ++count;
		}
		return count;
	}

		int VoxelWorld::dirtyCount() const {
			int count = 0;
			for (const auto& entry : chunks) {
				if (entry.second.loaded && entry.second.dirty) ++count;
			}
			return count;
		}

	int VoxelWorld::meshingCount() const {
		int count = 0;
		for (const auto& entry : regions) {
			if (entry.second.meshing) count += loadedChunksIn(entry.first);
		}
		return count;
	}

	int VoxelWorld::quadCount() const {
		int count = 0;
		for (const auto& entry : chunks) count += entry.second.quads;
		return count;
	}

	void VoxelWorld::clear() {
		for (const auto& entry : regions) removeRegionBodies(entry.first);
		chunks.clear();
		simulation.clearPieces();
		memoChunk = nullptr;
		dirtyRegions.clear();
	}

	// The blocks in flight are drawn as balls that move with them, in one runtime mesh that is rebuilt while any move.
	void VoxelWorld::setPreview(int x, int y, int z, int blockId) {
		clearPreview();
		if (blockId <= 0 || blockId >= 256) return;
		MeshArrays arrays;
		previewMesh(x, y, z, blockId, -1, arrays);
		showPreview(arrays);
	}

	void VoxelWorld::setPreviewProp(int x, int y, int z, int dir) {
		clearPreview();
		if (dir < 0 || dir > 5) return;
		MeshArrays arrays;
		previewMesh(x, y, z, 0, dir, arrays);
		showPreview(arrays);
	}

	void VoxelWorld::clearPreview() {
		if (!previewBody.first) return;
		if (Body* body = assets.get<Body>(previewBody.second)) removeRuntimeMesh(body);
		previewBody = { false, 0 };
	}

	void VoxelWorld::showPreview(MeshArrays& arrays) {
		if (arrays.indices.empty()) return;
		std::vector<id_t> materials;
		if (auto materialId = assets.getId<Material>("voxel_ghost")) {
			materials.push_back(materialId.value());
		} else {
			materials.push_back(assets.getDefaultId<Material>().value_or(0));
		}
		Body* body = createCompactMeshBody("voxel_preview", std::move(arrays.vertices), std::move(arrays.indices), materials,
			Vector3f(0.0f, 0.0f, 0.0f));
		if (body) {
			body->transparent = true;
			previewBody = { true, body->getId().value_or(0) };
		}
	}

	void VoxelWorld::previewMesh(int x, int y, int z, int blockId, int propDir, MeshArrays& out) const {
		// A padded copy of the blocks around the cell, PreviewRadius each side of it and up to just above it. Its origin is the
		// world cell (ox, 0, oz), so its mesh is local to that and is moved into place at the end.
		const int span = 2 * PreviewRadius;
		const int ox = x - PreviewRadius;
		const int oz = z - PreviewRadius;
		// Only the blocks a few below the cell are copied: the ghost keeps the surface within PreviewKeep of it, so the terrain far
		// below (up to the world's floor) is not needed.
		const int oy = std::max(0, y - PreviewRadius - 2);
		PaddedChunk padded;
		padded.size = span;
		padded.height = std::min(height, y + PreviewRadius + 1) - oy;
		padded.border = 3;
		const int p = span + 2 * padded.border;
		padded.blocks.assign(static_cast<size_t>(p) * p * (padded.height + 2 * padded.border), 0);
		for (int py = 0; py < padded.height; ++py) {
			for (int pz = -padded.border; pz < span + padded.border; ++pz) {
				for (int px = -padded.border; px < span + padded.border; ++px) {
					padded.blocks[padded.index(px, py, pz)] = static_cast<uint8_t>(getBlock(ox + px, oy + py, oz + pz));
				}
			}
		}
		for (size_t id = 0; id < 256; ++id) {
			padded.emission[id] = types[id].emits;
			padded.rule[id].group = meshGroup(types[id], static_cast<uint8_t>(id));
			padded.rule[id].join = types[id].joinAbove;
			padded.rule[id].joinBelow = types[id].joinBelow;
			padded.rule[id].cube = types[id].cube;
			padded.rule[id].blend = types[id].blend;
			padded.rule[id].passesLight = types[id].passesLight;
		}
		const int cx = x - ox;
		const int cy = y - oy;
		const int cz = z - oz;
		const std::array<glm::vec3, 256> colours = palette;
		const int resolution = resolutionFor(floorDiv(x, size), floorDiv(z, size));

		if (propDir >= 0) {
			padded.props.push_back({ cx, cy, cz, static_cast<uint8_t>(propDir), propAnchor(x, y, z, propDir), propNormal(x, y, z, propDir) });
			computeLight(padded);
			buildPropMesh(padded, out);
		} else if (blockId > 0) {
			const uint8_t group = padded.rule[static_cast<size_t>(blockId)].group;
			if (types[static_cast<size_t>(blockId)].cube || resolution <= 0) {
				// A cube is its own mesh: the copy holds only the new block, so nothing else in its group is drawn.
				PaddedChunk terrain = padded;
			computeSkin(terrain);
			std::fill(padded.blocks.begin(), padded.blocks.end(), 0);
				padded.blocks[padded.index(cx, cy, cz)] = static_cast<uint8_t>(blockId);
				computeLight(padded);
				buildChunkMesh(padded, colours, group, out);
				for (VoxelVertex& vertex : out.vertices) vertex.colour.a = ghostRise(terrain, vertex.position);
			} else {
				// The terrain as it is is kept for the filter below; the copy with the block added is meshed.
				PaddedChunk before = padded;
				padded.blocks[padded.index(cx, cy, cz)] = static_cast<uint8_t>(blockId);
				computeLight(padded);
				computeSkin(before);
				computeSkin(padded);
				MeshArrays full;
				buildSmoothMesh(padded, colours, resolution, 1, 0, group, groupBounds(padded)[group], full);
				const glm::vec3 centre(cx + 0.5f, cy + 0.5f, cz + 0.5f);
				// The triangles kept, and the vertices they use. The ghost keeps a little past PreviewKeep and fades out there.
				std::vector<bool> kept(full.vertices.size(), false);
				for (size_t t = 0; t + 2 < full.indices.size(); t += 3) {
					glm::vec3 corner(0.0f);
					for (size_t k = 0; k < 3; ++k) corner += full.vertices[full.indices[t + k]].position;
					const glm::vec3 middle = corner / 3.0f;
					if (glm::distance(middle, centre) > PreviewKeep * 1.2f) continue;
					// A triangle inside the terrain as it stands is the terrain itself, not new, so only the new ones are drawn.
					if (densityAtPoint(before, group, middle) > VoxelSkinIso) continue;
					for (size_t k = 0; k < 3; ++k) {
					out.indices.push_back(full.indices[t + k]);
					kept[full.indices[t + k]] = true;
				}
				}
				// The alpha of each kept vertex: it fades to nothing at the edge of the keep radius, and it fades in from the ground it stands on.
				for (size_t v = 0; v < full.vertices.size(); ++v) {
					if (!kept[v]) continue;
					VoxelVertex& vertex = full.vertices[v];
					const float edge = 1.0f - smoothRamp(PreviewKeep * 0.5f, PreviewKeep * 1.2f, glm::distance(vertex.position, centre));
					vertex.colour.a = edge * ghostRise(before, vertex.position);
				}
				out.vertices = std::move(full.vertices);
			}
		}
		// Move the ghost from the padded copy's corner into the world.
		for (VoxelVertex& vertex : out.vertices) {
			vertex.position.x += static_cast<float>(ox);
			vertex.position.y += static_cast<float>(oy);
			vertex.position.z += static_cast<float>(oz);
		}
	}


	glm::vec3 VoxelWorld::skinNormal(double x, double y, double z) const {
	    const double h = 0.25;
	    const double gx = skinDensity(x + h, y, z) - skinDensity(x - h, y, z);
	    const double gy = skinDensity(x, y + h, z) - skinDensity(x, y - h, z);
	    const double gz = skinDensity(x, y, z + h) - skinDensity(x, y, z - h);
	    const double length = std::sqrt(gx * gx + gy * gy + gz * gz);
	    if (length < 1e-9) return glm::vec3(0.0f, 1.0f, 0.0f);
	    return glm::vec3(static_cast<float>(-gx / length), static_cast<float>(-gy / length), static_cast<float>(-gz / length));
	}

	glm::vec3 VoxelWorld::propNormal(int x, int y, int z, int dir) const {
	    // The foot of the torch: on the skin, at the anchor toward the block it leans on from the cell's centre.
	    const double anchor = propAnchor(x, y, z, dir);
	    return skinNormal(x + 0.5 + PropDirs[dir][0] * anchor, y + 0.5 + PropDirs[dir][1] * anchor, z + 0.5 + PropDirs[dir][2] * anchor);
	}

	void VoxelWorld::updateMovingBlocks() {
		const std::vector<VoxelSimulation::Move>& moves = simulation.inFlight();
		if (moves.empty() && !movingShown) return;
		MeshArrays balls;
		for (const VoxelSimulation::Move& move : moves) {
			const double t = move.duration > 0.0 ? std::min(1.0, move.elapsed / move.duration) : 1.0;
			glm::vec3 corner(0.0f);
			for (int axis = 0; axis < 3; ++axis) {
				corner[axis] = static_cast<float>(move.from[axis] + (move.to[axis] - move.from[axis]) * t);
			}
			buildBlob(palette, move.block, corner + glm::vec3(0.5f), 0.42f, balls);
		}
		Body* body = movingBody.first ? assets.get<Body>(movingBody.second) : nullptr;
		if (body) {
			updateCompactMeshBody(body, std::move(balls.vertices), std::move(balls.indices));
		} else if (!balls.vertices.empty()) {
			std::vector<id_t> materials;
			if (auto materialId = assets.getId<Material>(material)) {
				materials.push_back(materialId.value());
			} else {
				materials.push_back(assets.getDefaultId<Material>().value_or(0));
			}
			body = createCompactMeshBody("voxel_moving_blocks", std::move(balls.vertices), std::move(balls.indices),
				materials, Vector3f(0.0f, 0.0f, 0.0f));
			if (body) movingBody = { true, body->getId().value_or(0) };
		}
		movingShown = !moves.empty();
	}
}
