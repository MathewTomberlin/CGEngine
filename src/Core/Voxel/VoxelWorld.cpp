#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <set>
#include <unordered_map>
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
		// The terrain installed per update, in milliseconds: an install costs a section's tops, dirty marks and neighbours, so a burst of them is spread out.
		const double InstallBudgetMs = 3.0;
		// Updates a change to the sections alone waits before the visibility is worked out again (see updateVisibility).
		const int VisibilityStampEvery = 4;
		// The dirty regions surveyed for a mesh start in one update (nearest first; see update).
		const size_t RegionScanLimit = 24;
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
			int cy = 0;
			int cz = 0;
			int resolution = 0;
			int stride = 1;
			unsigned skirtSides = 0;
			uint32_t generation = 0;
			std::shared_ptr<const ChunkMesh> reuse;   // the mesh the chunk kept, when it has not changed since: then it is not meshed again
		};

		// Adds a chunk's mesh to a region's, moved by the chunk's place in the region (offsets from the region's corner).
		void appendMesh(MeshArrays& into, const MeshArrays& part, float offsetX, float offsetY, float offsetZ) {
			const unsigned int base = static_cast<unsigned int>(into.vertices.size());
			into.vertices.reserve(into.vertices.size() + part.vertices.size());
			for (VoxelVertex vertex : part.vertices) {
				vertex.position.x += offsetX;
				vertex.position.y += offsetY;
				vertex.position.z += offsetZ;
				into.vertices.push_back(vertex);
			}
			into.indices.reserve(into.indices.size() + part.indices.size());
			for (unsigned int index : part.indices) into.indices.push_back(index + base);
		}
		// The faces the air of a section connects (Chunk::reach), from its blocks, or from its one block id when it is uniform. passes says which
		// blocks light goes through. Worked out on a worker, with the section's snapshot.
		std::array<uint8_t, 6> reachOf(const std::vector<uint8_t>& blocks, uint8_t fill, const std::array<bool, 256>& passes, int S) {
			const size_t N = static_cast<size_t>(S) * S * S;
			std::array<uint8_t, 6> reach{};
			if (blocks.empty()) {
				// A uniform section is all air (every face connects to every face) or all solid (none does).
				const bool open = fill == 0 || passes[fill];
				reach.fill(open ? 63 : 0);
				return reach;
			}
			auto passable = [&](size_t i) {
				const uint8_t block = blocks[i];
				return block == 0 || passes[block];
			};
			std::vector<uint8_t> seen(N, 0);
			std::vector<size_t> stack;
			for (size_t start = 0; start < N; ++start) {
				if (seen[start] || !passable(start)) continue;
				// One connected region of air: the faces it touches are connected to each other.
				uint8_t touched = 0;
				seen[start] = 1;
				stack.assign(1, start);
				while (!stack.empty()) {
					const size_t i = stack.back();
					stack.pop_back();
					const int lx = static_cast<int>(i % S);
					const int lz = static_cast<int>((i / S) % S);
					const int ly = static_cast<int>(i / (S * S));
					if (lx == S - 1) touched |= 1u << 0;
					if (lx == 0) touched |= 1u << 1;
					if (ly == S - 1) touched |= 1u << 2;
					if (ly == 0) touched |= 1u << 3;
					if (lz == S - 1) touched |= 1u << 4;
					if (lz == 0) touched |= 1u << 5;
					const int dx[6] = { 1, -1, 0, 0, 0, 0 }, dy[6] = { 0, 0, 1, -1, 0, 0 }, dz[6] = { 0, 0, 0, 0, 1, -1 };
					for (int d = 0; d < 6; ++d) {
						const int nx = lx + dx[d], ny = ly + dy[d], nz = lz + dz[d];
						if (nx < 0 || nx >= S || ny < 0 || ny >= S || nz < 0 || nz >= S) continue;
						const size_t j = static_cast<size_t>((ny * S + nz) * S + nx);
						if (seen[j] || !passable(j)) continue;
						seen[j] = 1;
						stack.push_back(j);
					}
				}
				for (int f = 0; f < 6; ++f) {
					if (touched & (1u << f)) reach[static_cast<size_t>(f)] |= touched;
				}
			}
			return reach;
		}

		const size_t MaxWorkers = 6;
		// A section whose blocks all lie this far under the terrain's surface is drawn with a stride of DeepStride (a quarter of the
		// quads at 2): the caves under the ground are most of the faces of the band, and the player sees them only up close.
		const int DeepDepth = 16;
		const int DeepStride = 2;
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

std::vector<VoxelVertex> VoxelWorld::probeGroundMesh(int cx, int cy, int cz, uint8_t group, int stride) const {
	PaddedChunk padded = snapshot(cx, cy, cz, VoxelLightReach);
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
	// A block is keyed by its coordinates, offset so each fits in its bits (the band keeps the piece near the player).
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
		if (result.visited >= maxBlocks) {
			result.status = 2;
			return result;
		}
		for (const auto& d : Dirs) {
			const int bx = block[0] + d[0], by = block[1] + d[1], bz = block[2] + d[2];
			// The band's edge holds the piece (the terrain beyond it is not checked further), as the edge of the loaded columns does.
			if (!loadedAt(bx, bz) || !withinBand(by)) {
				result.status = 0;
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
	if (!sectionLoaded(x, y, z)) return skinAt(x, y, z);
	const int block = getBlock(x, y, z);
	if (block == 0) return 0;
	const BlockType& type = types[static_cast<size_t>(block)];
	if (!type.solid) return 0;
	if (!type.own) return TerrainSkin;
	return skinAt(x, y, z);
}

int VoxelWorld::skinAt(int x, int y, int z) const {
	if (!sectionLoaded(x, y, z) && (inBand(floorDiv(y, size)) || !wanted(floorDiv(x, size), floorDiv(z, size)))) {
		// A section in the band that is not loaded is solid, and so is a column that is not wanted: a body stays in the loaded world.
		// Beyond the band the terrain is read as it is (getBlock).
		return TerrainSkin;
	}
	const int block = getBlock(x, y, z);
	if (block == 0) return 0;
	const BlockType& type = types[static_cast<size_t>(block)];
	if (!type.solid) return 0;
	if (!type.own) return TerrainSkin;
	// A block whose top is covered by its join mesh is in that mesh's skin (the same rule as the mesher's inGroup).
	uint8_t mesh = meshGroup(type, static_cast<uint8_t>(block));
	bool joinedAbove = false;
	if (type.joinAbove) {
		const int above = getBlock(x, y + 1, z);
		if (above != 0 && meshGroup(types[static_cast<size_t>(above)], static_cast<uint8_t>(above)) == type.joinAbove) {
			mesh = type.joinAbove;
			joinedAbove = true;
		}
	}
	// A block standing on a block of its join-below set is in the terrain's skin (the same rule as the mesher's inGroup).
	if (!joinedAbove && type.joinBelow.any() && type.joinBelow[static_cast<size_t>(getBlock(x, y - 1, z))]) {
		return TerrainSkin;
	}
	return (type.cube ? CubeSkin : OwnSkin) + mesh;
}

	const char* VoxelWorld::biomeName(int x, int z) const {
		switch (terrain.biomeAt(x, z)) {
		case VoxelTerrain::Biome::Forest: return "forest";
		case VoxelTerrain::Biome::Desert: return "desert";
		case VoxelTerrain::Biome::Sea: return "sea";
		default: return "plains";
		}
	}

uint8_t VoxelWorld::lightAt(int x, int y, int z) const {
	// Built on demand from the blocks around the cell, so it is a query, not the mesher's path.
	const Key key = sectionOf(x, y, z);
	PaddedChunk padded = snapshot(key[0], key[1], key[2], VoxelLightReach);
	computeLight(padded);
	const int lx = x - key[0] * size;
	const int ly = y - key[1] * size;
	const int lz = z - key[2] * size;
	return std::max(padded.lightAt(lx, ly, lz), padded.lampAt(lx, ly, lz));
}

int VoxelWorld::terrainCorners(int x, int y, int z) const {
	// The corner belongs to the section holding its block (x, y, z); its counts are kept there (see buildCounts).
	const Key key = sectionOf(x, y, z);
	const Chunk* chunk = findChunk(key);
	if (!chunk || !chunk->loaded) {
		// The unloaded sections count as skinAt sees them.
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
	if (chunk->counts.empty()) buildCounts(key[0], key[1], key[2]);
	const int j = y - key[1] * size;
	const int k = z - key[2] * size;
	const int i = x - key[0] * size;
	return chunk->counts[(static_cast<size_t>(j) * size + k) * size + i];
}

void VoxelWorld::buildCounts(int cx, int cy, int cz) const {
	const Chunk* chunk = findChunk(Key{ cx, cy, cz });
	if (!chunk) return;
	// The terrain occupancy of the blocks one before the section's corners and up to its top, then the 2 by 2 by 2 sum at each corner.
	const int U = size + 1;
	const int W = size + 1;
	const int V = size + 2;
	std::vector<uint8_t> solid(static_cast<size_t>(U) * W * V, 0);
	for (int v = 0; v < V; ++v) {
		for (int w = 0; w < W; ++w) {
			for (int u = 0; u < U; ++u) {
				solid[(static_cast<size_t>(v) * W + w) * U + u] = skinFast(cx * size + u - 1, cy * size + v - 1, cz * size + w - 1) == TerrainSkin ? 1 : 0;
			}
		}
	}
	chunk->counts.assign(static_cast<size_t>(size) * size * (size + 1), 0);
	for (int j = 0; j <= size; ++j) {
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
	// centres' occupancy, scaled by VoxelCentreWeight (the mesh's corners and centres are the same numbers). Each other smooth skin adds up
	// the weights of the corners it is in, and the density is the largest. The unloaded cells are solid terrain. Python's copy (VoxelPhysics.py)
	// matches it operation for operation, so the parity check can compare them.
	// The common case, read straight: the 3 by 3 by 3 blocks the density reads lie in one loaded section, away from its walls, and none is a
	// smooth block. Then the terrain is the section's corner counts and its blocks, which is the sum below with the same numbers (the creatures'
	// and the player's collision queries are this function, so it is the hot path).
	{
		const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y)), iz = static_cast<int>(std::floor(z));
		const Key key = sectionOf(ix, iy, iz);
		const int lx = ix - key[0] * size, ly = iy - key[1] * size, lz = iz - key[2] * size;
		const Chunk* chunk = findChunk(key);
		if (chunk && chunk->loaded && lx >= 1 && lx <= size - 2 && lz >= 1 && lz <= size - 2 && ly >= 1 && ly <= size - 2) {
			bool smooth = false;
			for (int dz = -1; dz <= 1 && !smooth; ++dz) {
				for (int dy = -1; dy <= 1 && !smooth; ++dy) {
					for (int dx = -1; dx <= 1 && !smooth; ++dx) {
						smooth = types[blockIn(*chunk, localIndex(lx + dx, ly + dy, lz + dz))].own;
					}
				}
			}
			if (!smooth) {
				if (chunk->counts.empty()) buildCounts(key[0], key[1], key[2]);
				// The centre cells are the block whose centre is the corner below the point, as in the sum below.
				const double bxFloor = std::floor(x - 0.5), byFloor = std::floor(y - 0.5), bzFloor = std::floor(z - 0.5);
				const int ox = static_cast<int>(bxFloor) - key[0] * size, oy = static_cast<int>(byFloor) - key[1] * size, oz = static_cast<int>(bzFloor) - key[2] * size;
				const double ux = x - ix, uy = y - iy, uz = z - iz;
				const double tx = x - 0.5 - bxFloor, ty = y - 0.5 - byFloor, tz = z - 0.5 - bzFloor;
				double terrainShare = 0.0;
				double centres = 0.0;
				for (int c = 0; c < 8; ++c) {
					const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
					const double w = (dx ? ux : 1.0 - ux) * (dy ? uy : 1.0 - uy) * (dz ? uz : 1.0 - uz);
					const size_t corner = (static_cast<size_t>(ly + dy) * size + (lz + dz)) * size + (lx + dx);
					terrainShare += w * (chunk->counts[corner] / 8.0 - VoxelTerrainShift);
					const double wc = (dx ? tx : 1.0 - tx) * (dy ? ty : 1.0 - ty) * (dz ? tz : 1.0 - tz);
					const uint8_t block = blockIn(*chunk, localIndex(ox + dx, oy + dy, oz + dz));
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
	// The column is loaded when any of its band's sections is.
	const int cx = floorDiv(x, size);
	const int cz = floorDiv(z, size);
	for (int cy = bandLow(); cy <= bandHigh(); ++cy) {
		const Chunk* chunk = findChunk(Key{ cx, cy, cz });
		if (chunk && chunk->loaded) return true;
	}
	return false;
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

void VoxelWorld::enqueue(int x, int y, int z, bool terrain, std::function<void()> job) {
	{
		std::lock_guard<std::mutex> lock(jobMutex);
		jobs.push_back(Job{ x, y, z, terrain, std::move(job) });
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

	int VoxelWorld::strideFor(int cx, int cy, int cz) const {
		// The surface comes from the generator, not the loaded sections, so the stride does not depend on which sections loaded first.
		const int top = terrain.columnHeight(cx * size + size / 2, cz * size + size / 2);
		const int deep = (cy + 1) * size <= top - DeepDepth ? DeepStride : 1;
		return std::max(strideFor(cx, cz), deep);
	}

	int VoxelWorld::resolutionFor(int cx, int cz) const {
		if (smoothResolution <= 0) return 0;
		if (nearRadius > 0 && focusKnown && std::max(std::abs(cx - focusCx), std::abs(cz - focusCz)) <= nearRadius) {
			return std::max(smoothResolution, nearResolution);
		}
		return smoothResolution;
	}

void VoxelWorld::markSectionsDirty(const std::vector<Key>& keys, bool neighbours) {
	// The same marks as markDirty and markNeighboursDirty for each key, but the regions are marked once each, not once per mark, and the
	// visibility is worked out again once.
	++visibilityStamp;
	std::set<Key> regionsMarked;
	auto mark = [this, &regionsMarked](const Key& key) {
		Chunk* chunk = findChunk(key);
		if (!chunk) return;
		chunk->counts.clear();
		chunk->dirty = true;
		regionsMarked.insert(regionOf(key));
	};
	for (const Key& key : keys) {
		mark(key);
		if (!neighbours) continue;
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					if (dx != 0 || dy != 0 || dz != 0) mark(Key{ key[0] + dx, key[1] + dy, key[2] + dz });
				}
			}
		}
	}
	for (const Key& region : regionsMarked) markRegionDirty(region);
}

void VoxelWorld::refreshResolutions(int focusX, int focusZ, bool force) {
	if (!force && focusKnown && focusX == focusCx && focusZ == focusCz) return;
	if (force) columnClasses.clear();
	std::vector<Key> changed;
	focusCx = focusX;
	focusCz = focusZ;
	focusKnown = true;
	// A section's level comes from its column (the resolution and the horizontal stride) and its own deep flag. A column's level changes only
	// where the focus's distance crosses a radius, so only the columns whose level changed have their sections looked at.
	for (const auto& column : columns) {
		const int wanted = resolutionFor(column.first, column.second);
		const int horizontal = strideFor(column.first, column.second);
		const std::pair<int, int> level{ wanted, horizontal };
		auto known = columnClasses.find(column);
		if (known != columnClasses.end() && known->second == level) continue;
		columnClasses[column] = level;
		for (int cy = bandLow() - BandKeep; cy <= bandHigh() + BandKeep; ++cy) {
			const Key key{ column.first, cy, column.second };
			auto found = chunks.find(key);
			if (found == chunks.end() || !found->second.loaded) continue;   // a section gets its level when its terrain is installed
			Chunk& chunk = found->second;
			const int wantedStride = std::max(horizontal, chunk.deep ? DeepStride : 1);
			if (wanted != chunk.resolution || wantedStride != chunk.stride) {
				chunk.resolution = wanted;
				chunk.stride = wantedStride;
				changed.push_back(key);
			}
		}
	}
	// The sections whose level changed, and their neighbours (whose seams with them change), are meshed again.
	if (!changed.empty()) markSectionsDirty(changed, true);
}

int VoxelWorld::chunkQuads(int cx, int cz) const {
	int total = 0;
	for (const auto& entry : chunks) {
		if (entry.first[0] == cx && entry.first[2] == cz && entry.second.loaded) total += entry.second.quads;
	}
	return total;
}

std::map<int, int> VoxelWorld::chunkGroupQuads(int cx, int cz, int yFrom, int yTo) const {
	std::map<int, int> total;
	for (const auto& entry : chunks) {
		if (entry.first[0] != cx || entry.first[2] != cz || !entry.second.loaded) continue;
		// A section counts when all of its blocks are in the range.
		if (entry.first[1] * size < yFrom || (entry.first[1] + 1) * size > yTo) continue;
		for (const auto& group : entry.second.groupQuads) total[group.first] += group.second;
	}
	return total;
}

std::map<int, int> VoxelWorld::chunkGroupQuads(int cx, int cz) const {
	std::map<int, int> total;
	for (const auto& entry : chunks) {
		if (entry.first[0] != cx || entry.first[2] != cz || !entry.second.loaded) continue;
		for (const auto& group : entry.second.groupQuads) total[group.first] += group.second;
	}
	return total;
}

	int VoxelWorld::floorDiv(int a, int b) {
		int q = a / b;
		if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
		return q;
	}

const VoxelWorld::Chunk* VoxelWorld::findChunk(const Key& key) const {
	if (memoChunk && memoKey == key) return memoChunk;
	auto it = chunks.find(key);
	if (it == chunks.end()) return nullptr;
	memoKey = key;
	memoChunk = &it->second;
	return memoChunk;
}

VoxelWorld::Chunk* VoxelWorld::findChunk(const Key& key) {
	auto it = chunks.find(key);
	return it == chunks.end() ? nullptr : &it->second;
}

int VoxelWorld::getBlock(int x, int y, int z) const {
	const Key key = sectionOf(x, y, z);
	const Chunk* chunk = findChunk(key);
	if (chunk && chunk->loaded) return blockIn(*chunk, localIndex(x - key[0] * size, y - key[1] * size, z - key[2] * size));
	// A section in the band that is not installed yet is air, as an unloaded column was. Beyond the band the terrain is read as it is
	// (for a wanted column): the edits first, then the generator.
	if (inBand(key[1]) || !wanted(key[0], key[2])) return 0;
	return analyticBlock(x, y, z);
}

	void VoxelWorld::setBlockQuiet(int x, int y, int z, int blockId) {
		const bool was = detachChecks;
		detachChecks = false;
		setBlock(x, y, z, blockId);
		detachChecks = was;
	}

void VoxelWorld::setBlock(int x, int y, int z, int blockId) {
	const Key key = sectionOf(x, y, z);
	const int lx = x - key[0] * size;
	const int ly = y - key[1] * size;
	const int lz = z - key[2] * size;
	const uint8_t block = static_cast<uint8_t>(std::clamp(blockId, 0, 255));
	clearDamage(x, y, z);
	// The edit is recorded even if the section is not loaded, so it is in place when the section loads. Edits are kept by section.
	edits[key][localIndex(lx, ly, lz)] = block;
	noteChange(x, y, z);
	Chunk* chunk = findChunk(key);
	if (!chunk || !chunk->loaded) return;
	writeBlock(*chunk, localIndex(lx, ly, lz), block);
	// A block near a section's face is seen from the neighbour too (its skin and its light reach that far), and near an edge or a corner
	// from the diagonal sections.
	markLight(x, y, z);
	changeTop(x, y, z, block);
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
	if (dir < 0 || dir > 5 || !loadedAt(x, z) || getBlock(x, y, z) != 0) return false;
	const Key key = sectionOf(x, y, z);
	propsBySection[key][localIndex(x - key[0] * size, y - key[1] * size, z - key[2] * size)] = static_cast<uint8_t>(dir);
	markLight(x, y, z);
	noteChange(x, y, z);
	return true;
}

void VoxelWorld::removeProp(int x, int y, int z) {
	const Key key = sectionOf(x, y, z);
	auto section = propsBySection.find(key);
	if (section == propsBySection.end()) return;
	if (section->second.erase(localIndex(x - key[0] * size, y - key[1] * size, z - key[2] * size)) == 0) return;
	if (section->second.empty()) propsBySection.erase(section);
	markLight(x, y, z);
	noteChange(x, y, z);
}

int VoxelWorld::propAt(int x, int y, int z) const {
	const Key key = sectionOf(x, y, z);
	auto section = propsBySection.find(key);
	if (section == propsBySection.end()) return -1;
	auto entry = section->second.find(localIndex(x - key[0] * size, y - key[1] * size, z - key[2] * size));
	return entry == section->second.end() ? -1 : entry->second;
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
	setDamage(x, y, z, stage);
	return stage;
}

int VoxelWorld::healBlock(int x, int y, int z) {
	const int stage = damageAt(x, y, z);
	if (stage <= 1) {
		clearDamage(x, y, z);
		return 0;
	}
	setDamage(x, y, z, stage - 1);
	return stage - 1;
}

int VoxelWorld::damageAt(int x, int y, int z) const {
	const Key key = sectionOf(x, y, z);
	auto section = damageBySection.find(key);
	if (section == damageBySection.end()) return 0;
	auto entry = section->second.find(localIndex(x - key[0] * size, y - key[1] * size, z - key[2] * size));
	return entry == section->second.end() ? 0 : entry->second;
}

void VoxelWorld::clearDamage(int x, int y, int z) {
	const Key key = sectionOf(x, y, z);
	auto section = damageBySection.find(key);
	if (section == damageBySection.end()) return;
	if (section->second.erase(localIndex(x - key[0] * size, y - key[1] * size, z - key[2] * size)) == 0) return;
	if (section->second.empty()) damageBySection.erase(section);
	markLight(x, y, z);
	noteChange(x, y, z);
}

void VoxelWorld::setDamage(int x, int y, int z, int stage) {
	if (stage <= 0) {
		clearDamage(x, y, z);
		return;
	}
	const Key key = sectionOf(x, y, z);
	damageBySection[key][localIndex(x - key[0] * size, y - key[1] * size, z - key[2] * size)] = static_cast<uint8_t>(stage);
	markLight(x, y, z);
	noteChange(x, y, z);
}

	void VoxelWorld::setRecordChanges(bool on) {
		recordChanges = on;
		simulation.setRecordStarts(on);
		if (!on) changedCells.clear();
	}

	std::vector<std::array<int, 3>> VoxelWorld::takeChanges() {
		std::vector<std::array<int, 3>> out(changedCells.begin(), changedCells.end());
		changedCells.clear();
		return out;
	}

std::vector<std::array<int, 3>> VoxelWorld::editedCells() const {
	std::set<std::array<int, 3>> cells;
	// A cell's key in a section is localIndex: (ly * size + lz) * size + lx, so x, y and z come back out of it.
	auto add = [&](const Key& key, int local) {
		const int lx = local % size;
		const int lz = (local / size) % size;
		const int ly = local / (size * size);
		cells.insert(std::array<int, 3>{ key[0] * size + lx, key[1] * size + ly, key[2] * size + lz });
	};
	for (const auto& section : edits) for (const auto& entry : section.second) add(section.first, entry.first);
	for (const auto& section : damageBySection) for (const auto& entry : section.second) add(section.first, entry.first);
	for (const auto& section : propsBySection) for (const auto& entry : section.second) add(section.first, entry.first);
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
	for (const auto& section : propsBySection) {
		for (const auto& entry : section.second) {
			const int lx = entry.first % size;
			const int lz = (entry.first / size) % size;
			const int ly = entry.first / (size * size);
			list.push_back({ section.first[0] * size + lx, section.first[1] * size + ly, section.first[2] * size + lz, entry.second });
		}
	}
	return list;
}

void VoxelWorld::markLight(int x, int y, int z) {
	// The sections whose box holds this cell: a cell within VoxelLightReach of a section's face is in the box of the section beyond it.
	const Key key = sectionOf(x, y, z);
	const int local[3] = { x - key[0] * size, y - key[1] * size, z - key[2] * size };
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dx = -1; dx <= 1; ++dx) {
				const int d[3] = { dx, dy, dz };
				bool reaches = true;
				for (int a = 0; a < 3; ++a) {
					if (d[a] == -1 && local[a] >= VoxelLightReach) reaches = false;
					if (d[a] == 1 && local[a] < size - VoxelLightReach) reaches = false;
				}
				if (reaches) markDirty(Key{ key[0] + dx, key[1] + dy, key[2] + dz });
			}
		}
	}
}

void VoxelWorld::fillColumn(int x, int z, int yFrom, int yTo, int blockId) {
	for (int y = yFrom; y < yTo; ++y) {
		setBlockQuiet(x, y, z, blockId);
	}
}

void VoxelWorld::markDirty(const Key& key) {
	++visibilityStamp;
	Chunk* chunk = findChunk(key);
	if (!chunk) return;
	chunk->counts.clear();   // the terrain's corner counts are out of date (terrainCorners)
	chunk->dirty = true;
	markRegionDirty(regionOf(key));
}

VoxelWorld::Key VoxelWorld::regionOf(const Key& key) {
	return { floorDiv(key[0], RegionChunks), floorDiv(key[1], RegionChunks), floorDiv(key[2], RegionChunks) };
}

void VoxelWorld::markRegionDirty(const Key& key) {
	Region& region = regions[key];
	region.settle = 0;
	dirtyRegions.insert(key);
}

void VoxelWorld::surveyRegion(const Key& key, int& loaded, bool& settled) const {
	// One lookup per section of the region and of the ring one section around it: 0 absent, 1 loaded, 2 requested and still waiting for its
	// terrain. The region's loaded sections count, and it is settled when none of them has a requested neighbour (the 26 around it).
	constexpr int Side = RegionChunks + 2;
	uint8_t state[Side][Side][Side];
	for (int lz = 0; lz < Side; ++lz) {
		for (int ly = 0; ly < Side; ++ly) {
			for (int lx = 0; lx < Side; ++lx) {
				const Chunk* chunk = findChunk(Key{ key[0] * RegionChunks + lx - 1, key[1] * RegionChunks + ly - 1, key[2] * RegionChunks + lz - 1 });
				state[lz][ly][lx] = !chunk ? 0 : (chunk->loaded ? 1 : 2);
			}
		}
	}
	loaded = 0;
	settled = true;
	for (int lz = 1; lz <= RegionChunks; ++lz) {
		for (int ly = 1; ly <= RegionChunks; ++ly) {
			for (int lx = 1; lx <= RegionChunks; ++lx) {
				if (state[lz][ly][lx] != 1) continue;
				++loaded;
				for (int dz = -1; dz <= 1; ++dz) {
					for (int dy = -1; dy <= 1; ++dy) {
						for (int dx = -1; dx <= 1; ++dx) {
							if (state[lz + dz][ly + dy][lx + dx] == 2) settled = false;
						}
					}
				}
			}
		}
	}
}

int VoxelWorld::loadedChunksIn(const Key& key) const {
	int count = 0;
	for (int lz = 0; lz < RegionChunks; ++lz) {
		for (int ly = 0; ly < RegionChunks; ++ly) {
			for (int lx = 0; lx < RegionChunks; ++lx) {
				const Chunk* chunk = findChunk(Key{ key[0] * RegionChunks + lx, key[1] * RegionChunks + ly, key[2] * RegionChunks + lz });
				if (chunk && chunk->loaded) ++count;
			}
		}
	}
	return count;
}

void VoxelWorld::removeRegionBodies(const Key& key) {
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

void VoxelWorld::markNeighboursDirty(const Key& key) {
	for (int dz = -1; dz <= 1; ++dz) {
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				if (dx != 0 || dy != 0 || dz != 0) markDirty(Key{ key[0] + dx, key[1] + dy, key[2] + dz });
			}
		}
	}
}

bool VoxelWorld::requestChunk(int cx, int cz) {
	columns.insert(std::make_pair(cx, cz));
	bool requested = false;
	for (int cy = bandLow(); cy <= bandHigh(); ++cy) {
		if (chunks.count(Key{ cx, cy, cz })) continue;
		requestSection(Key{ cx, cy, cz });
		requested = true;
	}
	return requested;
}

bool VoxelWorld::isLoaded(int cx, int cz) const {
	for (int cy = bandLow(); cy <= bandHigh(); ++cy) {
		const Chunk* chunk = findChunk(Key{ cx, cy, cz });
		if (!chunk || !chunk->loaded) return false;
	}
	return true;
}

void VoxelWorld::unloadChunk(int cx, int cz) {
	columns.erase(std::make_pair(cx, cz));
	std::vector<Key> own;
	for (const auto& entry : chunks) {
		if (entry.first[0] == cx && entry.first[2] == cz) own.push_back(entry.first);
	}
	dropSections(own);
}

void VoxelWorld::installTerrain(TerrainResult& result) {
	const Key key{ result.cx, result.cy, result.cz };
	auto it = chunks.find(key);
	if (it == chunks.end() || it->second.generation != result.generation || it->second.loaded) return;
	Chunk& chunk = it->second;
	applyEdits(key, result.blocks);
	storeBlocks(chunk, std::move(result.blocks));
	chunk.loaded = true;
	chunk.resolution = resolutionFor(result.cx, result.cz);
	// Deep is read from the generator once, here; a refresh for a new focus then needs only the section's own flag.
	chunk.deep = (result.cy + 1) * size <= terrain.columnHeight(result.cx * size + size / 2, result.cz * size + size / 2) - DeepDepth;
	chunk.stride = std::max(strideFor(result.cx, result.cz), chunk.deep ? DeepStride : 1);
	// The section's roofs raise the column's tops, and its faces, and those of the neighbours that face it, are meshed from the new blocks.
	updateTops(key, chunk);
	markDirty(key);
	markNeighboursDirty(key);
}

void VoxelWorld::applyEdits(const Key& key, std::vector<uint8_t>& blocks) const {
	// The edits of the section are kept by its local index, so the ones it holds are the ones it takes.
	auto section = edits.find(key);
	if (section == edits.end()) return;
	for (const auto& edit : section->second) blocks[static_cast<size_t>(edit.first)] = edit.second;
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
	// Each entry is the absolute x, y, z (int32) and then the value: a block id, or a torch's dir, or a damage stage.
	const auto absolute = [this](const Key& key, int local) {
		return std::array<int32_t, 3>{ key[0] * size + local % size, key[1] * size + local / (size * size), key[2] * size + (local / size) % size };
	};
	out.write("CGVE", 4);
	put32(3);   // version 3: the blocks, then the torches, then the damage
	put32(static_cast<uint32_t>(editCount()));
	for (const auto& section : edits) {
		for (const auto& edit : section.second) {
			const auto at = absolute(section.first, edit.first);
			put32(static_cast<uint32_t>(at[0]));
			put32(static_cast<uint32_t>(at[1]));
			put32(static_cast<uint32_t>(at[2]));
			out.put(static_cast<char>(edit.second));
		}
	}
	// The torches, after the blocks: a count, then x, y, z (int32) and dir (a byte) for each.
	uint32_t torches = 0;
	for (const auto& section : propsBySection) torches += static_cast<uint32_t>(section.second.size());
	put32(torches);
	for (const auto& section : propsBySection) {
		for (const auto& entry : section.second) {
			const auto at = absolute(section.first, entry.first);
			put32(static_cast<uint32_t>(at[0]));
			put32(static_cast<uint32_t>(at[1]));
			put32(static_cast<uint32_t>(at[2]));
			out.put(static_cast<char>(entry.second));
		}
	}
	// The damage, after the torches: a count, then x, y, z (int32) and the stage (a byte) for each damaged block.
	uint32_t damaged = 0;
	for (const auto& section : damageBySection) damaged += static_cast<uint32_t>(section.second.size());
	put32(damaged);
	for (const auto& section : damageBySection) {
		for (const auto& entry : section.second) {
			const auto at = absolute(section.first, entry.first);
			put32(static_cast<uint32_t>(at[0]));
			put32(static_cast<uint32_t>(at[1]));
			put32(static_cast<uint32_t>(at[2]));
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
	// The file is the whole state of the edits, so an edit made since the save is undone by the load. The file is read into its
	// own maps first, so a damaged file changes nothing.
	using SectionMap = std::map<Key, std::map<int, uint8_t>>;
	SectionMap fileEdits;
	SectionMap fileProps;
	SectionMap fileDamage;
	// An entry's place in the file: absolute x, y, z, and the section and local index it is kept by.
	const auto place = [this](int32_t wx, int32_t wy, int32_t wz, Key& key, int& local) {
		key = sectionOf(wx, wy, wz);
		local = localIndex(wx - key[0] * size, wy - key[1] * size, wz - key[2] * size);
	};
	for (uint32_t i = 0; i < count; ++i) {
		uint32_t x = 0, y = 0, z = 0;
		if (!get32(x) || !get32(y) || !get32(z)) return false;
		const int block = in.get();
		if (block == std::char_traits<char>::eof()) return false;
		Key key;
		int local = 0;
		place(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z), key, local);
		fileEdits[key][local] = static_cast<uint8_t>(block);
	}
	// Version 2 carries the torches after the blocks (see saveEdits). They are kept like the edits, by section.
	if (version >= 2) {
		uint32_t torches = 0;
		if (!get32(torches)) return false;
		for (uint32_t i = 0; i < torches; ++i) {
			uint32_t x = 0, y = 0, z = 0;
			if (!get32(x) || !get32(y) || !get32(z)) return false;
			const int dir = in.get();
			if (dir == std::char_traits<char>::eof()) return false;
			if (dir > 5) continue;
			Key key;
			int local = 0;
			place(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z), key, local);
			fileProps[key][local] = static_cast<uint8_t>(dir);
		}
	}
	// Version 3 carries the damage after the torches: the stage of each damaged block, kept by section like the torches.
	if (version >= 3) {
		uint32_t damaged = 0;
		if (!get32(damaged)) return false;
		for (uint32_t i = 0; i < damaged; ++i) {
			uint32_t x = 0, y = 0, z = 0;
			if (!get32(x) || !get32(y) || !get32(z)) return false;
			const int stage = in.get();
			if (stage == std::char_traits<char>::eof()) return false;
			if (stage <= 0) continue;
			Key key;
			int local = 0;
			place(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z), key, local);
			fileDamage[key][local] = static_cast<uint8_t>(stage);
		}
	}

	// Every section that had edits, or has some in the file, is looked at. A loaded one is made again from its terrain and then the file's
	// edits, so an edit the file lacks shows the terrain again. A section not loaded takes the file's edits when it loads.
	std::set<Key> touched;
	for (const SectionMap* map : { &edits, &propsBySection, &damageBySection, &fileEdits, &fileProps, &fileDamage }) {
		for (const auto& section : *map) touched.insert(section.first);
	}
	edits = std::move(fileEdits);
	propsBySection = std::move(fileProps);
	damageBySection = std::move(fileDamage);
	std::set<std::pair<int, int>> columnsTouched;
	for (const Key& key : touched) {
		columnsTouched.insert(std::make_pair(key[0], key[2]));
		Chunk* chunk = findChunk(key);
		if (!chunk || !chunk->loaded) continue;
		std::vector<uint8_t> blocks;
		terrain.fillSection(key[0], key[1], key[2], size, blocks);
		applyEdits(key, blocks);
		storeBlocks(*chunk, std::move(blocks));
		markDirty(key);
		markNeighboursDirty(key);
	}
	for (const auto& column : columnsTouched) recomputeTops(column.first, column.second);
	return true;
}

VoxelWorld::SnapshotSource VoxelWorld::gather(const Key& origin, const std::array<int, 3>& dims, int border) const {
	// The state a snapshot reads, copied here: the sections (blocks, or the terrain's edits), the column tops, the props, the damage, and the
	// block rules. The worker builds from these copies alone (buildPadded), so the world may change meanwhile.
	SnapshotSource source;
	source.size = size;
	source.border = border;
	source.origin = origin;
	source.dims = dims;
	source.terrain = terrain;
	for (size_t id = 0; id < 256; ++id) {
		source.emission[id] = types[id].emits;
		source.passesLight[id] = types[id].passesLight;
		PaddedChunk::MeshRule& rule = source.rule[id];
		rule.group = meshGroup(types[id], static_cast<uint8_t>(id));
		rule.join = types[id].joinAbove;
		rule.joinBelow = types[id].joinBelow;
		rule.cube = types[id].cube;
		rule.blend = types[id].blend;
		rule.passesLight = types[id].passesLight;
		rule.textured = types[id].textured;
		rule.splat = types[id].splat;
		for (int k = 0; k < 3; ++k) rule.tiles[k] = types[id].tiles[k];
	}
	const int nx = dims[0], ny = dims[1], nz = dims[2];
	source.sections.resize(static_cast<size_t>(nx) * ny * nz);
	for (int dy = 0; dy < ny; ++dy) {
		for (int dz = 0; dz < nz; ++dz) {
			for (int dx = 0; dx < nx; ++dx) {
				const Key key{ origin[0] + dx, origin[1] + dy, origin[2] + dz };
				SectionSource& out = source.sections[static_cast<size_t>((dy * nz + dz) * nx + dx)];
				const Chunk* chunk = findChunk(key);
				// A loaded section is its blocks. A section beyond the band, of a wanted column, is the terrain with its edits (see buildPadded).
				if (chunk && chunk->loaded) {
					out.kind = SectionSource::Kind::Loaded;
					out.fill = chunk->fill;
					out.blocks = chunk->blocks;
				} else if (!inBand(key[1]) && wanted(key[0], key[2])) {
					out.kind = SectionSource::Kind::Analytic;
					auto section = edits.find(key);
					if (section != edits.end()) out.edits = section->second;
				}
			}
		}
	}
	// The column tops of the gathered columns, in one grid (topAt for each column of the box).
	source.topX = origin[0] * size;
	source.topZ = origin[2] * size;
	source.topWidth = nx * size;
	source.tops.assign(static_cast<size_t>(nx * size) * static_cast<size_t>(nz * size), NoSolid);
	for (int sz = 0; sz < nz; ++sz) {
		for (int sx = 0; sx < nx; ++sx) {
			auto column = tops.find(std::make_pair(origin[0] + sx, origin[2] + sz));
			if (column == tops.end()) continue;
			for (int lz = 0; lz < size; ++lz) {
				for (int lx = 0; lx < size; ++lx) {
					source.tops[static_cast<size_t>((sz * size + lz) * source.topWidth + sx * size + lx)] = column->second[static_cast<size_t>(lz * size + lx)];
				}
			}
		}
	}
	// The torches and the damage of the gathered sections, in world coordinates. A prop's anchor and normal read the world's density, so
	// they are worked out here.
	for (int dy = 0; dy < ny; ++dy) {
		for (int dz = 0; dz < nz; ++dz) {
			for (int dx = 0; dx < nx; ++dx) {
				const Key key{ origin[0] + dx, origin[1] + dy, origin[2] + dz };
				auto section = propsBySection.find(key);
				if (section != propsBySection.end()) {
					for (const auto& entry : section->second) {
						const int wx = key[0] * size + entry.first % size;
						const int wz = key[2] * size + (entry.first / size) % size;
						const int wy = key[1] * size + entry.first / (size * size);
						source.props.push_back({ wx, wy, wz, entry.second, propAnchor(wx, wy, wz, entry.second), propNormal(wx, wy, wz, entry.second) });
					}
				}
				auto damaged = damageBySection.find(key);
				if (damaged != damageBySection.end()) {
					for (const auto& entry : damaged->second) {
						const int wx = key[0] * size + entry.first % size;
						const int wz = key[2] * size + (entry.first / size) % size;
						const int wy = key[1] * size + entry.first / (size * size);
						const int hits = std::max(1, static_cast<int>(types[static_cast<size_t>(getBlock(wx, wy, wz))].hits));
						source.damage.push_back({ wx, wy, wz, static_cast<uint8_t>(std::min(255, entry.second * 255 / hits)) });
					}
				}
			}
		}
	}
	return source;
}

PaddedChunk VoxelWorld::buildPadded(const SnapshotSource& source, int cx, int cy, int cz, std::map<Key, std::vector<uint8_t>>& analytic) {
	const int size = source.size;
	const int border = source.border;
	PaddedChunk padded;
	padded.size = size;
	padded.height = size;
	padded.border = border;
	const int p = size + 2 * border;
	padded.blocks.assign(static_cast<size_t>(p) * p * (size + 2 * border), 0);

	// The 27 sections around (cx, cy, cz), indexed by the offset of the section from it: [dy + 1][dz + 1][dx + 1]. A section beyond the band
	// is the terrain as it is, with its edits: generated here once per key (analytic), so the sections that share it share the work.
	struct View {
		bool present = false;
		uint8_t fill = 0;
		const std::vector<uint8_t>* blocks = nullptr;
	};
	View around[3][3][3];
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dx = -1; dx <= 1; ++dx) {
				const Key key{ cx + dx, cy + dy, cz + dz };
				const SectionSource& section = source.section(key);
				View& view = around[dy + 1][dz + 1][dx + 1];
				if (section.kind == SectionSource::Kind::Loaded) {
					view.present = true;
					view.fill = section.fill;
					if (!section.blocks.empty()) view.blocks = &section.blocks;
				} else if (section.kind == SectionSource::Kind::Analytic) {
					auto found = analytic.find(key);
					if (found == analytic.end()) {
						std::vector<uint8_t> blocks;
						source.terrain.fillSection(key[0], key[1], key[2], size, blocks, false);
						for (const auto& edit : section.edits) blocks[static_cast<size_t>(edit.first)] = edit.second;
						found = analytic.emplace(key, std::move(blocks)).first;
					}
					view.present = true;
					view.blocks = &found->second;
				}
			}
		}
	}
	const auto local = [size](int x, int y, int z) { return (y * size + z) * size + x; };
	// Copied a run at a time: a row along x stays in one neighbour except where it crosses a section border, so each row takes at most
	// three copies.
	for (int y = -border; y < size + border; ++y) {
		const int py = y + border;
		const int ny = y < 0 ? -1 : (y >= size ? 1 : 0);
		const int inY = y - ny * size;
		for (int pz = 0; pz < p; ++pz) {
			const int lz = pz - border;
			const int nz = lz < 0 ? -1 : (lz >= size ? 1 : 0);
			const int inZ = lz - nz * size;
			for (int px = 0; px < p;) {
				const int lx = px - border;
				const int nx = lx < 0 ? -1 : (lx >= size ? 1 : 0);
				const int inX = lx - nx * size;
				const int run = nx < 0 ? -lx : (nx == 0 ? size - lx : p - px);
				const View& view = around[ny + 1][nz + 1][nx + 1];
				if (view.present) {
					uint8_t* out = &padded.blocks[static_cast<size_t>((py * p + pz) * p + px)];
					if (!view.blocks) {
						std::memset(out, view.fill, static_cast<size_t>(run));
					} else {
						std::memcpy(out, &(*view.blocks)[static_cast<size_t>(local(inX, inY, inZ))], static_cast<size_t>(run));
					}
				}
				px += run;
			}
		}
	}
	// The torches that reach into the padded box (PaddedChunk::props), in the section's coordinates.
	for (const PropCopy& prop : source.props) {
		const int px = prop.x - cx * size, py = prop.y - cy * size, pz = prop.z - cz * size;
		if (px < -border || px >= size + border || py < -border || py >= size + border || pz < -border || pz >= size + border) continue;
		padded.props.push_back({ px, py, pz, prop.dir, prop.anchor, prop.normal });
	}
	// The damage of the blocks in the padded area, as the fraction of their hits taken (PaddedChunk::damage).
	for (const DamageCopy& entry : source.damage) {
		const int px = entry.x - cx * size, py = entry.y - cy * size, pz = entry.z - cz * size;
		if (px < -border || px >= size + border || py < -border || py >= size + border || pz < -border || pz >= size + border) continue;
		if (padded.damage.empty()) padded.damage.assign(padded.blocks.size(), 0);
		padded.damage[padded.index(px, py, pz)] = entry.value;
	}
	// The sky above the box, column by column: open where no roof is above the box's top (PaddedChunk::sky).
	const int above = cy * size + size + border;
	padded.sky.assign(static_cast<size_t>(p) * p, 0);
	const int topRows = static_cast<int>(source.tops.size()) / source.topWidth;
	for (int pz = 0; pz < p; ++pz) {
		for (int px = 0; px < p; ++px) {
			const int gx = cx * size + px - border - source.topX;
			const int gz = cz * size + pz - border - source.topZ;
			const bool inside = gx >= 0 && gx < source.topWidth && gz >= 0 && gz < topRows;
			const int top = inside ? source.tops[static_cast<size_t>(gz * source.topWidth + gx)] : NoSolid;
			padded.sky[static_cast<size_t>(pz * p + px)] = top < above ? 1 : 0;
		}
	}
	padded.emission = source.emission;
	padded.rule = source.rule;
	return padded;
}

PaddedChunk VoxelWorld::snapshot(int cx, int cy, int cz, int border) const {
	// The 3 by 3 by 3 sections around (cx, cy, cz), copied and built here (the same as a worker builds them).
	std::map<Key, std::vector<uint8_t>> analytic;
	return buildPadded(gather(Key{ cx - 1, cy - 1, cz - 1 }, { 3, 3, 3 }, border), cx, cy, cz, analytic);
}

void VoxelWorld::startMeshing(const Key& key) {
	Region& region = regions[key];
	region.meshing = true;
	dirtyRegions.erase(key);
	// Each loaded section of the region is listed here, with its detail and, when it has not changed, the mesh it was built to. The sections that
	// changed (or have no mesh) are built on a worker from the copy gather makes here, so a section is not built again while nothing changed.
	std::vector<ChunkInput> inputs;
	bool anyBuilt = false;
	for (int lz = 0; lz < RegionChunks; ++lz) {
		for (int ly = 0; ly < RegionChunks; ++ly) {
			for (int lx = 0; lx < RegionChunks; ++lx) {
				const int cx = key[0] * RegionChunks + lx;
				const int cy = key[1] * RegionChunks + ly;
				const int cz = key[2] * RegionChunks + lz;
				Chunk* chunk = findChunk(Key{ cx, cy, cz });
				if (!chunk || !chunk->loaded) continue;
				ChunkInput input;
				input.cx = cx;
				input.cy = cy;
				input.cz = cz;
				input.generation = chunk->generation;
				input.resolution = chunk->resolution;
				input.stride = chunk->stride;
				// Skirts on the sides that meet a neighbour at another level of detail (bit 1: -x, 2: +x, 4: -z, 8: +z).
				if (input.resolution > 0) {
					const int dxs[4] = { -1, 1, 0, 0 };
					const int dzs[4] = { 0, 0, -1, 1 };
					for (int side = 0; side < 4; ++side) {
						const Chunk* neighbour = findChunk(Key{ cx + dxs[side], cy, cz + dzs[side] });
						if (neighbour && neighbour->loaded && (neighbour->resolution != input.resolution || neighbour->stride != input.stride)) {
							input.skirtSides |= 1u << side;
						}
					}
				}
				if (chunk->dirty || !chunk->mesh) {
					anyBuilt = true;
				} else {
					input.reuse = chunk->mesh;
				}
				chunk->dirty = false;
				inputs.push_back(std::move(input));
			}
		}
	}
	// The region with one section each side (a section's snapshot reads its neighbours, and its border lies within them).
	SnapshotSource source;
	if (anyBuilt) {
		source = gather(Key{ key[0] * RegionChunks - 1, key[1] * RegionChunks - 1, key[2] * RegionChunks - 1 },
			{ RegionChunks + 2, RegionChunks + 2, RegionChunks + 2 }, VoxelLightReach);
	}
	const std::array<glm::vec3, 256> colours = palette;
	std::array<bool, 256> cubeShape{};
	for (size_t id = 0; id < 256; ++id) cubeShape[id] = types[id].cube;
	const uint32_t epoch = region.epoch;
	enqueue(key[0] * RegionChunks + RegionChunks / 2, key[1] * RegionChunks + RegionChunks / 2, key[2] * RegionChunks + RegionChunks / 2, false,
		[this, key, epoch, inputs = std::move(inputs), source = std::move(source), colours, cubeShape]() mutable {
		const auto began = std::chrono::steady_clock::now();
		RegionResult result;
		result.region = key;
		result.epoch = epoch;
		std::map<int, MeshArrays> merged;
		std::map<int, int> mergedQuads;
		const auto stage = [](std::chrono::steady_clock::time_point since) {
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - since).count();
		};
		// The analytic sections beyond the band are generated once for the whole region, not once per snapshot that reads them.
		std::map<Key, std::vector<uint8_t>> analytic;
		for (ChunkInput& input : inputs) {
			std::shared_ptr<const ChunkMesh> mesh = input.reuse;
			std::array<uint8_t, 6> reach{};
			bool reachKnown = false;
			if (!mesh) {
				auto clock = std::chrono::steady_clock::now();
				PaddedChunk padded = buildPadded(source, input.cx, input.cy, input.cz, analytic);
				const SectionSource& own = source.section(Key{ input.cx, input.cy, input.cz });
				reach = reachOf(own.blocks, own.fill, source.passesLight, source.size);
				reachKnown = true;
				clock = std::chrono::steady_clock::now();
				computeLight(padded);
				result.lightMs += stage(clock);
				clock = std::chrono::steady_clock::now();
				if (input.resolution > 0) computeSkin(padded);
				result.skinMs += stage(clock);
				clock = std::chrono::steady_clock::now();
				auto built = std::make_shared<ChunkMesh>();
				// The terrain, and a mesh of its own for each block type in the section that has one (see PaddedChunk::rule).
				// The groups: the terrain, and each block type with blocks in the section. A smooth mesh takes the groups' bounds, which
				// need the skin (computeSkin); a section of cubes has no skin, so it finds its groups from the rules.
				std::array<GroupBounds, 256> bounds{};
				std::array<bool, 256> present{};
				if (input.resolution > 0) {
					bounds = groupBounds(padded);
					for (size_t id = 1; id < 256; ++id) present[id] = bounds[id].lo[0] <= bounds[id].hi[0];
				} else {
					for (uint8_t block : padded.blocks) {
						if (padded.rule[block].group != 0) present[padded.rule[block].group] = true;
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
						buildChunkMesh(padded, colours, group, part);
					} else {
						buildSmoothMesh(padded, colours, input.resolution, input.stride, input.skirtSides, group, bounds[group], part);
					}
					// The torches are drawn with the terrain (their cells are air to it, so they are not part of any block's mesh).
					if (group == 0) buildPropMesh(padded, part);
				}
				result.surfaceMs += stage(clock);
				++result.built;
				mesh = built;
			}
			// The section's place in the region: its meshes are local to the section, and the region's body is at its corner.
			const float offsetX = static_cast<float>((input.cx - key[0] * RegionChunks) * source.size);
			const float offsetY = static_cast<float>((input.cy - key[1] * RegionChunks) * source.size);
			const float offsetZ = static_cast<float>((input.cz - key[2] * RegionChunks) * source.size);
			for (const auto& entry : mesh->groups) {
				appendMesh(merged[entry.first], entry.second, offsetX, offsetY, offsetZ);
				mergedQuads[entry.first] += static_cast<int>(entry.second.indices.size() / 6);
			}
			result.chunks.push_back({ input.cx, input.cy, input.cz, input.generation, mesh, reach, reachKnown });
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

std::string VoxelWorld::bodyName(const Key& region, uint8_t group) const {
	std::string name = "voxel_region_" + std::to_string(region[0]) + "_" + std::to_string(region[1]) + "_" + std::to_string(region[2]);
	if (group != 0) name += "_" + std::to_string(group);
	return name;
}

void VoxelWorld::uploadMesh(RegionResult& result) {
	const Key key = result.region;
	auto it = regions.find(key);
	if (it == regions.end()) return;
	Region& region = it->second;
	region.meshing = false;
	// The region's bodies were removed while it was being meshed, so this mesh is dropped.
	if (result.epoch != region.epoch) return;
	++uploads;
	// Each section keeps the mesh it was built to, so the next region job reuses it. A section that changed while its mesh was
	// built is dirty again, so its region is meshed again after this upload; until then the body is one change behind.
	for (const ChunkBuilt& built : result.chunks) {
		Chunk* chunk = findChunk(Key{ built.cx, built.cy, built.cz });
		if (!chunk || !chunk->loaded || chunk->generation != built.generation) continue;
		if (built.reachKnown && chunk->reach != built.reach) {
			chunk->reach = built.reach;
			++visibilityStamp;   // the camera's view of the sections is worked out again (see updateVisibility)
		}
		chunk->mesh = built.mesh;
		chunk->quads = 0;
		chunk->groupQuads.clear();
		for (const auto& entry : built.mesh->groups) {
			const int quads = static_cast<int>(entry.second.indices.size() / 6);
			chunk->quads += quads;
			if (!entry.second.vertices.empty()) chunk->groupQuads[entry.first] = quads;
		}
	}

	const Vector3f position(static_cast<float>(key[0] * RegionChunks * size), static_cast<float>(key[1] * RegionChunks * size),
		static_cast<float>(key[2] * RegionChunks * size));
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
			body = createCompactMeshBody(bodyName(key, part.group), std::move(part.mesh.vertices),
				std::move(part.mesh.indices), materials, position);
			if (body) region.bodies[part.group] = body->getId().value_or(0);
		}
		if (body) {
			body->transparent = types[part.group].blend;
			body->setRenderingEnabled(region.visible);
		}
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

int VoxelWorld::update(double budgetMs, int focusX, int focusZ, int focusY) {
	const auto phaseBegan = std::chrono::steady_clock::now();
	auto phase = [&](int slot) {
		const auto now = std::chrono::steady_clock::now();
		lastPhases[static_cast<size_t>(slot)] = std::chrono::duration<double, std::milli>(now - phaseBegan).count();
	};
	setJobFocus(focusX, focusZ);
	refreshResolutions(focusX, focusZ, false);
	const int wantedCy = floorDiv(focusY, size);
	if (!bandKnown || wantedCy != focusCy) {
		focusCy = wantedCy;
		bandKnown = true;
		reconcileBand();
	}
	phase(0);
	updateVisibility(focusX, focusCy, focusZ);
	phase(1);
	updateMovingBlocks();
	// 1. Install the terrain that finished generating, until its budget is spent (at least one). The rest waits, in order, for the next update.
	std::deque<TerrainResult> installed;
	{
		std::lock_guard<std::mutex> lock(resultMutex);
		installed.swap(terrainResults);
	}
	const auto installBegan = std::chrono::steady_clock::now();
	while (!installed.empty()) {
		TerrainResult& result = installed.front();
		totals.terrain += result.ms;
		installTerrain(result);
		installed.pop_front();
		const double spent = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - installBegan).count();
		if (spent >= InstallBudgetMs) break;
	}
	if (!installed.empty()) {
		std::lock_guard<std::mutex> lock(resultMutex);
		terrainResults.insert(terrainResults.begin(), installed.begin(), installed.end());
	}
	totals.install += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - installBegan).count();
	phase(2);

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
	phase(3);
	const auto scanBegan = std::chrono::steady_clock::now();

	// 3. Start meshing the changed regions nearest the focus, once they have settled. A region costs its loaded sections against
	//    MaxMeshStartsPerUpdate, but the first one always starts, so the queue moves.
	// The dirty regions, nearest the focus first. Each is counted towards its settling; only the nearest RegionScanLimit that are ready are
	// surveyed in an update (a survey is a lookup per section around the region), so a burst of dirty regions is looked at over a few updates.
	std::vector<std::pair<int, Key>> ordered;
	for (const Key& key : dirtyRegions) {
		const int dx = key[0] * RegionChunks + RegionChunks / 2 - focusX;
		const int dz = key[2] * RegionChunks + RegionChunks / 2 - focusZ;
		ordered.push_back({ dx * dx + dz * dz, key });
	}
	std::sort(ordered.begin(), ordered.end(), [](const std::pair<int, Key>& a, const std::pair<int, Key>& b) {
		return a.first < b.first;
	});
	std::vector<std::pair<int, Key>> candidates;
	int surveys = 0;
	for (const auto& [distance, key] : ordered) {
		Region& region = regions[key];
		if (region.meshing) continue;
		if (++region.settle < RegionSettleUpdates) continue;
		if (surveys >= RegionScanLimit) continue;
		++surveys;
		// The survey is the scan's cost, so it runs for a region that is ready to start.
		int loaded = 0;
		bool settled = false;
		surveyRegion(key, loaded, settled);
		if (loaded == 0) {
			dirtyRegions.erase(key);
			continue;
		}
		if (settled) candidates.push_back({ distance, key });
	}
	std::sort(candidates.begin(), candidates.end(), [](const std::pair<int, Key>& a, const std::pair<int, Key>& b) {
		return a.first < b.first;
	});
	const auto startBegan = std::chrono::steady_clock::now();
	lastPhases[5] = std::chrono::duration<double, std::milli>(startBegan - scanBegan).count();
	// The work of a start is the dirty sections it meshes, so the budget counts them (their snapshots are built on the workers).
	int budget = MaxMeshStartsPerUpdate;
	for (size_t i = 0; i < candidates.size(); ++i) {
		const Key key = candidates[i].second;
		int work = 0;
		for (int lz = 0; lz < RegionChunks; ++lz) {
			for (int ly = 0; ly < RegionChunks; ++ly) {
				for (int lx = 0; lx < RegionChunks; ++lx) {
					const Chunk* chunk = findChunk(Key{ key[0] * RegionChunks + lx, key[1] * RegionChunks + ly, key[2] * RegionChunks + lz });
					if (chunk && chunk->loaded && (chunk->dirty || !chunk->mesh)) ++work;
				}
			}
		}
		if (i > 0 && work > budget) continue;
		startMeshing(key);
		budget -= work;
	}
	totals.start += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startBegan).count();
	lastPhases[4] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startBegan).count();
	return uploaded;
}

	VoxelWorld::Counts VoxelWorld::counts() const {
	// The four counts in one pass over the sections (the state file reads them every quarter second).
	Counts out;
	for (const auto& entry : chunks) {
		const Chunk& chunk = entry.second;
		if (!chunk.loaded) {
			++out.pending;
			continue;
		}
		++out.loaded;
		if (chunk.dirty) ++out.dirty;
		out.quads += chunk.quads;
	}
	return out;
}

int VoxelWorld::loadedCount() const {
		int count = 0;
		for (const auto& entry : chunks) {
			if (entry.second.loaded) ++count;
		}
		return count;
	}

std::pair<double, uint64_t> VoxelWorld::benchTerrain(int side, int bandLow, int bandHigh) const {
	const auto began = std::chrono::steady_clock::now();
	uint64_t checksum = 0;
	std::vector<uint8_t> blocks;
	for (int cz = 0; cz < side; ++cz) {
		for (int cx = 0; cx < side; ++cx) {
			for (int cy = bandLow; cy <= bandHigh; ++cy) {
				terrain.fillSection(cx, cy, cz, size, blocks);
				for (size_t i = 0; i < blocks.size(); ++i) checksum = checksum * 1099511628211ull + blocks[i] + i;
			}
		}
	}
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
	return { ms, checksum };
}

std::vector<int> VoxelWorld::readyCounts(int cx, int cz, const std::vector<int>& radii) const {
	// One pass over the sections, bucketed by their distance (in chunks) from (cx, cz), then a running total for each radius. The state file
	// asks for these every quarter second, so they must not cost a lookup per cell of each square.
	int widest = 0;
	for (int radius : radii) widest = std::max(widest, radius);
	std::set<Key> meshingRegions;
	for (const auto& entry : regions) {
		if (entry.second.meshing) meshingRegions.insert(entry.first);
	}
	std::vector<int> byDistance(static_cast<size_t>(widest) + 1, 0);
	for (const auto& entry : chunks) {
		const Key& key = entry.first;
		const Chunk& chunk = entry.second;
		if (!chunk.loaded || chunk.dirty || !chunk.mesh) continue;
		const int distance = std::max(std::abs(key[0] - cx), std::abs(key[2] - cz));
		if (distance > widest) continue;
		if (key[1] < bandLow() || key[1] > bandHigh()) continue;
		if (!meshingRegions.empty() && meshingRegions.count(regionOf(key))) continue;
		++byDistance[static_cast<size_t>(distance)];
	}
	std::vector<int> counts;
	for (int radius : radii) {
		int total = 0;
		for (int d = 0; d <= radius; ++d) total += byDistance[static_cast<size_t>(d)];
		counts.push_back(total);
	}
	return counts;
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
	tops.clear();
	columns.clear();
	bandKnown = false;
	windowKnown = false;
	columnClasses.clear();
	simulation.clearPieces();
	memoChunk = nullptr;
	dirtyRegions.clear();
}

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
		const int oy = y - PreviewRadius - 2;
		PaddedChunk padded;
		padded.size = span;
		padded.height = y + PreviewRadius + 1 - oy;
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

bool VoxelWorld::sectionLoaded(int x, int y, int z) const {
	const Chunk* chunk = findChunk(sectionOf(x, y, z));
	return chunk && chunk->loaded;
}

bool VoxelWorld::roofs(uint8_t block) const {
	// The sky stops at a block that light does not pass (air and the water pass it, as they do in computeLight).
	return block != 0 && !types[block].passesLight;
}

int VoxelWorld::topAt(int x, int z) const {
	const int cx = floorDiv(x, size);
	const int cz = floorDiv(z, size);
	auto column = tops.find(std::make_pair(cx, cz));
	if (column == tops.end()) return NoSolid;
	return column->second[static_cast<size_t>((z - cz * size) * size + (x - cx * size))];
}

void VoxelWorld::updateTops(const Key& key, const Chunk& chunk) {
	auto column = tops.find(std::make_pair(key[0], key[2]));
	if (column == tops.end()) column = tops.emplace(std::make_pair(key[0], key[2]), std::vector<int>(static_cast<size_t>(size) * size, NoSolid)).first;
	bool changed = false;
	for (int lz = 0; lz < size; ++lz) {
		for (int lx = 0; lx < size; ++lx) {
			for (int ly = size - 1; ly >= 0; --ly) {
				if (!roofs(blockIn(chunk, localIndex(lx, ly, lz)))) continue;
				const int y = key[1] * size + ly;
				int& top = column->second[static_cast<size_t>(lz * size + lx)];
				if (y > top) {
					top = y;
					changed = true;
				}
				break;
			}
		}
	}
	if (changed) markTopChanged(key[0], key[2]);
}

void VoxelWorld::changeTop(int x, int y, int z, uint8_t block) {
	const int cx = floorDiv(x, size);
	const int cz = floorDiv(z, size);
	const int lx = x - cx * size;
	const int lz = z - cz * size;
	auto column = tops.find(std::make_pair(cx, cz));
	if (column == tops.end()) {
		if (!roofs(block)) return;   // no roof in this column and none placed: nothing to record
		column = tops.emplace(std::make_pair(cx, cz), std::vector<int>(static_cast<size_t>(size) * size, NoSolid)).first;
	}
	int& top = column->second[static_cast<size_t>(lz * size + lx)];
	if (roofs(block) && y > top) {
		top = y;
		markTopChanged(cx, cz);
	} else if (!roofs(block) && y == top) {
		// The roof came out: the column's next roof below, within the band, is the new top.
		top = NoSolid;
		for (int yy = y - 1; yy >= bandLow() * size; --yy) {
			if (roofs(getBlock(x, yy, z))) {
				top = yy;
				break;
			}
		}
		markTopChanged(cx, cz);
	}
}

void VoxelWorld::recomputeTops(int cx, int cz) {
	auto column = tops.find(std::make_pair(cx, cz));
	if (column == tops.end()) return;
	std::vector<int> fresh(static_cast<size_t>(size) * size, NoSolid);
	for (int cy = bandHigh(); cy >= bandLow(); --cy) {
		const Chunk* chunk = findChunk(Key{ cx, cy, cz });
		if (!chunk || !chunk->loaded) continue;
		for (int lz = 0; lz < size; ++lz) {
			for (int lx = 0; lx < size; ++lx) {
				int& top = fresh[static_cast<size_t>(lz * size + lx)];
				if (top != NoSolid) continue;
				for (int ly = size - 1; ly >= 0; --ly) {
					if (roofs(blockIn(*chunk, localIndex(lx, ly, lz)))) {
						top = cy * size + ly;
						break;
					}
				}
			}
		}
	}
	if (fresh != column->second) {
		column->second = std::move(fresh);
		markTopChanged(cx, cz);
	}
}

bool VoxelWorld::findRoute(const std::array<int, 3>& start, const std::array<int, 3>& goal, const std::vector<int>& liquids,
	std::vector<std::array<int, 3>>& route) const {
	// The same search as VoxelPaths.find_path: the same radius, cell cap, neighbour order and standing rule.
	const int Radius = 14;
	const size_t MaxCells = 2000;
	static const int Steps[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	route.clear();
	auto goalAt = [&](int x, int y, int z) {
		return std::abs(x - goal[0]) <= 1 && std::abs(z - goal[2]) <= 1 && std::abs(y - goal[1]) <= 1;
	};
	if (goalAt(start[0], start[1], start[2])) return true;
	std::array<bool, 256> liquid{};
	for (int id : liquids) {
		if (id >= 0 && id < 256) liquid[static_cast<size_t>(id)] = true;
	}
	auto standsAt = [&](int x, int y, int z) {
		if (getBlock(x, y, z) != 0 || getBlock(x, y + 1, z) != 0) return false;
		const int below = getBlock(x, y - 1, z);
		return below != 0 && !(below >= 0 && below < 256 && liquid[static_cast<size_t>(below)]);
	};
	// A cell's key: its coordinates packed into one number (each fits its bits: the world is far under a million blocks high).
	auto key = [](int x, int y, int z) {
		return (static_cast<uint64_t>(x + (1 << 20)) << 42) | (static_cast<uint64_t>(y + (1 << 20)) << 21) | static_cast<uint64_t>(z + (1 << 20));
	};
	std::vector<std::array<int, 3>> cells{ start };
	std::vector<int> parent{ -1 };
	std::unordered_map<uint64_t, int> index{ { key(start[0], start[1], start[2]), 0 } };
	std::deque<int> queue{ 0 };
	int found = -1;
	while (!queue.empty() && cells.size() < MaxCells && found < 0) {
		const int at = queue.front();
		queue.pop_front();
		const std::array<int, 3> cell = cells[static_cast<size_t>(at)];
		for (const auto& step : Steps) {
			const int nx = cell[0] + step[0], nz = cell[2] + step[1];
			if (std::abs(nx - start[0]) > Radius || std::abs(nz - start[2]) > Radius) continue;
			for (int ny : { cell[1], cell[1] - 1, cell[1] + 1 }) {
				if (index.count(key(nx, ny, nz))) continue;
				if (ny == cell[1] + 1 && getBlock(cell[0], cell[1] + 2, cell[2]) != 0) continue;   // no room over the head to step up
				if (!standsAt(nx, ny, nz)) continue;
				cells.push_back({ nx, ny, nz });
				parent.push_back(at);
				const int added = static_cast<int>(cells.size()) - 1;
				index[key(nx, ny, nz)] = added;
				if (goalAt(nx, ny, nz)) {
					found = added;
					break;
				}
				queue.push_back(added);
			}
			if (found >= 0) break;
		}
	}
	if (found < 0) return false;
	for (int at = found; parent[static_cast<size_t>(at)] != -1; at = parent[static_cast<size_t>(at)]) {
		route.push_back(cells[static_cast<size_t>(at)]);
	}
	std::reverse(route.begin(), route.end());
	return true;
}

void VoxelWorld::updateClouds(double timeSeconds, int focusX, int focusZ) {
	// The sheet's height, above the islands (VoxelTerrain::IslandMax) and the band of a player near the ground.
	const float CloudY = 100.0f;
	// The sheet covers this many blocks each way from the focus, in cells of CloudCell blocks; a cloud cell is drawn when its cover is high.
	const int Reach = 128;
	const int CloudCell = 4;
	const float CloudCover = 0.56f;
	const double RebuildSeconds = 0.5;
	if (cloudBuiltAt > -999.0 && timeSeconds - cloudBuiltAt < RebuildSeconds) return;
	cloudBuiltAt = timeSeconds;
	MeshArrays sheet;
	const int x0 = (focusX - Reach) / CloudCell * CloudCell, z0 = (focusZ - Reach) / CloudCell * CloudCell;
	const int cells = 2 * Reach / CloudCell;
	for (int j = 0; j < cells; ++j) {
		for (int i = 0; i < cells; ++i) {
			const int wx = x0 + i * CloudCell, wz = z0 + j * CloudCell;
			if (terrain.cloudCover(wx + CloudCell / 2, wz + CloudCell / 2, timeSeconds) < CloudCover) continue;
			const unsigned int base = static_cast<unsigned int>(sheet.vertices.size());
			const float corners[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
			for (const auto& c : corners) {
				VoxelVertex vertex;
				vertex.position = glm::vec3(static_cast<float>(wx) + c[0] * CloudCell, CloudY, static_cast<float>(wz) + c[1] * CloudCell);
				vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
				vertex.colour = glm::vec4(1.0f, 1.0f, 1.0f, 0.85f);
				sheet.vertices.push_back(vertex);
			}
			// Both faces, so the sheet shows from above and below.
			sheet.indices.insert(sheet.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3,
				base, base + 2, base + 1, base, base + 3, base + 2 });
		}
	}
	Body* body = cloudBody.first ? assets.get<Body>(cloudBody.second) : nullptr;
	if (sheet.vertices.empty()) {
		if (body) removeRuntimeMesh(body);
		cloudBody = { false, 0 };
		return;
	}
	if (body) {
		updateCompactMeshBody(body, std::move(sheet.vertices), std::move(sheet.indices));
	} else {
		std::vector<id_t> materials;
		if (auto materialId = assets.getId<Material>(material)) {
			materials.push_back(materialId.value());
		} else {
			materials.push_back(assets.getDefaultId<Material>().value_or(0));
		}
		body = createCompactMeshBody("voxel_clouds", std::move(sheet.vertices), std::move(sheet.indices), materials,
			Vector3f(0.0f, 0.0f, 0.0f));
		if (body) {
			body->transparent = true;
			cloudBody = { true, body->getId().value_or(0) };
		}
	}
}

void VoxelWorld::updateVisibility(int cx, int cy, int cz) {
	// Worked out again when the camera's section changes or any section does: a section is seen when the camera's section reaches it
	// through open air, through the faces each section's air connects (Chunk::reach). A section not reached is occluded. The walk marks
	// the seen sections with an epoch number, so nothing is cleared, and it is cheap enough to run on a frame the camera moves.
	const Key from{ cx, cy, cz };
	if (visibilitySeen == visibilityStamp && visibilityFrom == from) return;
	// The walk waits a few updates after a change (the camera's section, or the sections alone, which streaming bumps most updates), so a run of
	// changes, such as flying across sections, is one walk; a section that is seen a few updates late is at worst drawn a few updates late.
	if (++visibilitySkips < VisibilityStampEvery) return;
	visibilitySkips = 0;
	visibilitySeen = visibilityStamp;
	visibilityFrom = from;
	const uint32_t epoch = ++visitEpoch;
	auto pack = [](const Key& k) {
		return (static_cast<uint64_t>(k[0] + (1 << 20)) << 42) | (static_cast<uint64_t>(k[1] + (1 << 20)) << 21) | static_cast<uint64_t>(k[2] + (1 << 20));
	};
	const int dx[6] = { 1, -1, 0, 0, 0, 0 }, dy[6] = { 0, 0, 1, -1, 0, 0 }, dz[6] = { 0, 0, 0, 0, 1, -1 };
	// The loaded sections, in a flat grid over their box: the walk reads each neighbour by index, where a map lookup per step (over thousands of
	// sections, scattered in memory) cost most of a frame. Each section reached, and the faces it was entered through (bit f: entered from
	// face f's side).
	// The box is the wanted columns and the band (plus the keep range): every loaded section is in it, so it is not found by a scan of them all.
	Key low = from;
	Key high = from;
	for (const auto& column : columns) {
		low[0] = std::min(low[0], column.first);
		high[0] = std::max(high[0], column.first);
		low[2] = std::min(low[2], column.second);
		high[2] = std::max(high[2], column.second);
	}
	low[1] = std::min(low[1], bandLow() - BandKeep);
	high[1] = std::max(high[1], bandHigh() + BandKeep);
	const int spanX = high[0] - low[0] + 1, spanZ = high[2] - low[2] + 1;
	auto indexOf = [&](const Key& k) {
		return static_cast<size_t>(((k[1] - low[1]) * spanZ + (k[2] - low[2])) * spanX + (k[0] - low[0]));
	};
	std::vector<Chunk*> grid(static_cast<size_t>(spanX) * (high[1] - low[1] + 1) * spanZ, nullptr);
	for (auto& entry : chunks) {
		const Key& key = entry.first;
		if (!entry.second.loaded) continue;
		if (key[0] < low[0] || key[0] > high[0] || key[1] < low[1] || key[1] > high[1] || key[2] < low[2] || key[2] > high[2]) continue;
		grid[indexOf(key)] = &entry.second;
	}
	std::vector<uint8_t> entered(grid.size(), 0);
	std::vector<Key> queue;
	const bool startKnown = grid[indexOf(from)] != nullptr;
	if (startKnown) {
		entered[indexOf(from)] = 63;
		queue.push_back(from);
	}
	for (size_t head = 0; head < queue.size(); ++head) {
		const Key key = queue[head];
		const size_t at = indexOf(key);
		Chunk* chunk = grid[at];
		chunk->visitEpoch = epoch;
		const uint8_t in = entered[at];
		// The faces the air reaches from the faces it was entered through.
		uint8_t out = 0;
		for (int f = 0; f < 6; ++f) {
			if (in & (1u << f)) out |= chunk->reach[static_cast<size_t>(f)];
		}
		for (int g = 0; g < 6; ++g) {
			if (!(out & (1u << g))) continue;
			// Into the neighbour through face g; the neighbour is entered through the face opposite g.
			const Key next{ key[0] + dx[g], key[1] + dy[g], key[2] + dz[g] };
			if (next[0] < low[0] || next[0] > high[0] || next[1] < low[1] || next[1] > high[1] || next[2] < low[2] || next[2] > high[2]) continue;
			const size_t j = indexOf(next);
			if (!grid[j]) continue;
			const uint8_t arrive = static_cast<uint8_t>(1u << (g ^ 1));
			if (entered[j] & arrive) continue;
			entered[j] |= arrive;
			queue.push_back(next);
		}
	}
	// A region is drawn when one of its sections is seen (the walk marked them with this epoch); a region only changes its drawing when its
	// seen state does. Its sections are read from the grid, so no set of the seen regions is built.
	for (auto& entry : regions) {
		bool seen = !startKnown;
		for (int lz = 0; lz < RegionChunks && !seen; ++lz) {
			for (int ly = 0; ly < RegionChunks && !seen; ++ly) {
				for (int lx = 0; lx < RegionChunks && !seen; ++lx) {
					const Key key{ entry.first[0] * RegionChunks + lx, entry.first[1] * RegionChunks + ly, entry.first[2] * RegionChunks + lz };
					if (key[0] < low[0] || key[0] > high[0] || key[1] < low[1] || key[1] > high[1] || key[2] < low[2] || key[2] > high[2]) continue;
					const Chunk* chunk = grid[indexOf(key)];
					if (chunk && chunk->visitEpoch == epoch) seen = true;
				}
			}
		}
		const bool visible = seen;
		if (entry.second.visible == visible) continue;
		entry.second.visible = visible;
		for (const auto& body : entry.second.bodies) {
			if (Body* mesh = assets.get<Body>(body.second)) mesh->setRenderingEnabled(visible);
		}
	}
}

void VoxelWorld::markTopChanged(int cx, int cz) {
	// The sky above a column reaches the sections beside it too, so the sections of the 3 by 3 columns around are meshed again.
	std::vector<Key> keys;
	for (int dz = -1; dz <= 1; ++dz) {
		for (int dx = -1; dx <= 1; ++dx) {
			for (int cy = bandLow(); cy <= bandHigh(); ++cy) {
				const Key key{ cx + dx, cy, cz + dz };
				const Chunk* chunk = findChunk(key);
				if (chunk && chunk->loaded) keys.push_back(key);
			}
		}
	}
	// One batch: a column's top changes as each of its sections installs, so this runs for every one of them.
	markSectionsDirty(keys, false);
}

void VoxelWorld::storeBlocks(Chunk& chunk, std::vector<uint8_t>&& blocks) {
	const uint8_t first = blocks.empty() ? 0 : blocks[0];
	if (std::all_of(blocks.begin(), blocks.end(), [first](uint8_t block) { return block == first; })) {
		chunk.blocks.clear();   // a uniform section keeps one block id, not the array
		chunk.fill = first;
	} else {
		chunk.blocks = std::move(blocks);
	}
}

void VoxelWorld::writeBlock(Chunk& chunk, int index, uint8_t block) {
	if (chunk.blocks.empty()) {
		if (chunk.fill == block) return;
		chunk.blocks.assign(static_cast<size_t>(size) * size * size, chunk.fill);
	}
	chunk.blocks[static_cast<size_t>(index)] = block;
}

uint8_t VoxelWorld::analyticBlock(int x, int y, int z) const {
	uint8_t block = 0;
	if (editAt(x, y, z, block)) return block;
	return terrain.cellBlock(x, y, z, terrain.columnAt(x, z));
}

bool VoxelWorld::editAt(int x, int y, int z, uint8_t& out) const {
	const Key key = sectionOf(x, y, z);
	auto section = edits.find(key);
	if (section == edits.end()) return false;
	auto entry = section->second.find(localIndex(x - key[0] * size, y - key[1] * size, z - key[2] * size));
	if (entry == section->second.end()) return false;
	out = entry->second;
	return true;
}

bool VoxelWorld::inBand(int cy) const {
	return std::abs(cy - focusCy) <= bandSections;
}

bool VoxelWorld::withinBand(int y) const {
	return inBand(floorDiv(y, size));
}

bool VoxelWorld::wanted(int cx, int cz) const {
	return columns.count(std::make_pair(cx, cz)) > 0;
}

int VoxelWorld::bandLow() const {
	return focusCy - bandSections;
}

int VoxelWorld::bandHigh() const {
	return focusCy + bandSections;
}

void VoxelWorld::requestSection(const Key& key) {
	Chunk& chunk = chunks[key];
	chunk.generation = nextGeneration++;
	const uint32_t generation = chunk.generation;
	const VoxelTerrain settings = terrain;
	const int chunkSize = size;
	const int cx = key[0], cy = key[1], cz = key[2];
	enqueue(cx, cy, cz, true, [this, settings, cx, cy, cz, generation, chunkSize] {
		const auto began = std::chrono::steady_clock::now();
		TerrainResult result;
		result.cx = cx;
		result.cy = cy;
		result.cz = cz;
		result.generation = generation;
		settings.fillSection(cx, cy, cz, chunkSize, result.blocks);
		result.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
		std::lock_guard<std::mutex> lock(resultMutex);
		terrainResults.push_back(std::move(result));
	});
}

void VoxelWorld::dropSections(const std::vector<Key>& keys) {
	// Dropped together, so the work of a band's worth of sections is one pass over the queued jobs and one check of each column's roofs,
	// not one of each per section.
	std::set<Key> gone;
	for (const Key& key : keys) {
		if (chunks.erase(key) > 0) gone.insert(key);
	}
	if (gone.empty()) return;
	memoChunk = nullptr;
	// A queued job for them is not removed (a scan of every queued job for each layer cost milliseconds): it runs, far from the focus, and its
	// result is ignored, since the section it is for is gone (installTerrain checks the generation).
	// A column's roofs change only when a dropped section held its highest roof (the tops are the highest roof of each cell): so a column is
	// worked out again only then, and not for every section that goes.
	std::map<std::pair<int, int>, std::set<int>> droppedHeights;
	std::set<Key> regionsTouched;
	for (const Key& key : gone) {
		droppedHeights[std::make_pair(key[0], key[2])].insert(key[1]);
		regionsTouched.insert(regionOf(key));
	}
	for (const auto& column : droppedHeights) {
		auto top = tops.find(column.first);
		if (top == tops.end()) continue;
		for (int value : top->second) {
			if (value == NoSolid) continue;
			if (column.second.count(floorDiv(value, size))) {
				recomputeTops(column.first.first, column.first.second);
				break;
			}
		}
	}
	// The regions still have these sections in them, so they are meshed again without them (or removed, when they are empty).
	for (const Key& region : regionsTouched) {
		if (loadedChunksIn(region) == 0) {
			removeRegionBodies(region);
		} else {
			markRegionDirty(region);
		}
	}
	// The neighbours of the dropped sections (their faces against them change) are marked in one batch: the dropped ones are gone, so only the neighbours are dirtied.
	markSectionsDirty(std::vector<Key>(gone.begin(), gone.end()), true);
}

void VoxelWorld::reconcileBand() {
	// The sections out of the band go, and the band's sections of the wanted columns are requested. A section goes only once it is
	// BandKeep sections out of the band, so a focus that bounces across a section line does not rebuild the band each time.
	const int low = bandLow();
	const int high = bandHigh();
	if (!windowKnown) {
		// The first reconcile (or a new band size, or a cleared world): every section is looked at once.
		std::vector<Key> stale;
		for (const auto& entry : chunks) {
			const int away = std::abs(entry.first[1] - focusCy) - bandSections;
			if (away > BandKeep || !wanted(entry.first[0], entry.first[2])) stale.push_back(entry.first);
		}
		dropSections(stale);
		for (const auto& column : columns) {
			for (int cy = low; cy <= high; ++cy) {
				const Key key{ column.first, cy, column.second };
				if (!chunks.count(key)) requestSection(key);
			}
		}
	} else {
		// A focus that moved a section up or down: only the heights that left the keep range go, and only the heights that entered the band
		// are requested. Every wanted column keeps its sections within the keep range (see requestChunk and unloadChunk), so nothing else changes.
		const int oldKeepLow = reconciledLow - BandKeep, oldKeepHigh = reconciledHigh + BandKeep;
		const int newKeepLow = low - BandKeep, newKeepHigh = high + BandKeep;
		std::vector<Key> leaving;
		auto forHeights = [this, &leaving](int from, int to, bool drop) {
			for (int cy = from; cy <= to; ++cy) {
				for (const auto& column : columns) {
					const Key key{ column.first, cy, column.second };
					if (drop) {
						if (chunks.count(key)) leaving.push_back(key);
					} else if (!chunks.count(key)) {
						requestSection(key);
					}
				}
			}
		};
		forHeights(oldKeepLow, std::min(oldKeepHigh, newKeepLow - 1), true);
		forHeights(std::max(oldKeepLow, newKeepHigh + 1), oldKeepHigh, true);
		dropSections(leaving);
		forHeights(low, std::min(high, reconciledLow - 1), false);
		forHeights(std::max(low, reconciledHigh + 1), high, false);
	}
	reconciledLow = low;
	reconciledHigh = high;
	windowKnown = true;
}
}
