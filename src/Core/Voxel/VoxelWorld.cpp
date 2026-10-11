#include <algorithm>
#include <cstdlib>
#include <chrono>
#include "VoxelWorld.h"
#include "../Engine/Engine.h"
#include "../Mesh/RuntimeMesh.h"
#include "../Body/Body.h"

namespace CGEngine {
	namespace {
		// Dirty chunks mesh in this many per update at most, so one update never copies too many chunks.
		const int MaxMeshStartsPerUpdate = 16;
		const size_t MaxWorkers = 6;
	}

	VoxelWorld::VoxelWorld(int chunkSize, int chunkHeight, const std::string& materialName)
		: size(chunkSize), height(chunkHeight), material(materialName) {
		palette.fill(glm::vec3(1.0f));
		// The terrain's own blocks: sand falls, and water is a liquid that is not solid. Air is not solid either.
		types.fill(BlockType());
		types[0].solid = false;
		types[VoxelTerrain::Sand].falls = true;
		types[Water::Source] = BlockType{ false, false, true };
		for (int id = Water::FlowFirst; id <= Water::FlowLast; ++id) types[static_cast<size_t>(id)] = BlockType{ false, false, true };
		startWorkers();
	}

	void VoxelWorld::defineBlock(int blockId, const BlockType& type) {
		types[static_cast<size_t>(std::clamp(blockId, 0, 255))] = type;
	}

	bool VoxelWorld::isSolid(int x, int y, int z) const {
		const int block = getBlock(x, y, z);
		return block != 0 && types[static_cast<size_t>(block)].solid;
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
				job = std::move(jobs.front());
				jobs.pop_front();
			}
			job();
		}
	}

	void VoxelWorld::enqueue(std::function<void()> job) {
		{
			std::lock_guard<std::mutex> lock(jobMutex);
			jobs.push_back(std::move(job));
		}
		jobReady.notify_one();
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
			if (wanted != chunk.resolution) {
				chunk.resolution = wanted;
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

	int VoxelWorld::floorDiv(int a, int b) {
		int q = a / b;
		if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
		return q;
	}

	const VoxelWorld::Chunk* VoxelWorld::findChunk(int cx, int cz) const {
		auto it = chunks.find(std::make_pair(cx, cz));
		return it == chunks.end() ? nullptr : &it->second;
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

	void VoxelWorld::setBlock(int x, int y, int z, int blockId) {
		if (y < 0 || y >= height) return;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		const int lx = x - cx * size;
		const int lz = z - cz * size;
		const uint8_t block = static_cast<uint8_t>(std::clamp(blockId, 0, 255));
		// The edit is recorded even if the chunk is not loaded, so it is in place when the chunk loads.
		edits[std::make_pair(cx, cz)][localIndex(lx, y, lz)] = block;
		Chunk* chunk = findChunk(cx, cz);
		if (!chunk || !chunk->loaded) return;
		chunk->blocks[localIndex(lx, y, lz)] = block;
		// A block on a chunk border is seen from the neighbour too, and a corner from the three diagonal chunks.
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dz = -1; dz <= 1; ++dz) {
				if (dx == -1 && lx != 0) continue;
				if (dx == 1 && lx != size - 1) continue;
				if (dz == -1 && lz != 0) continue;
				if (dz == 1 && lz != size - 1) continue;
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
	}

	void VoxelWorld::fillColumn(int x, int z, int yFrom, int yTo, int blockId) {
		for (int y = std::max(0, yFrom); y < std::min(height, yTo); ++y) {
			setBlock(x, y, z, blockId);
		}
	}

	void VoxelWorld::markDirty(int cx, int cz) {
		if (Chunk* chunk = findChunk(cx, cz)) {
			chunk->dirty = true;
			++chunk->version;
			dirtyKeys.insert(std::make_pair(cx, cz));
		}
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
		enqueue([this, settings, cx, cz, generation, chunkSize, chunkHeight] {
			TerrainResult result;
			result.cx = cx;
			result.cz = cz;
			result.generation = generation;
			settings.fillChunk(cx, cz, chunkSize, chunkHeight, result.blocks);
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
		if (it->second.body.first) {
			if (Body* body = assets.get<Body>(it->second.body.second)) removeRuntimeMesh(body);
		}
		dirtyKeys.erase(std::make_pair(cx, cz));
		chunks.erase(it);
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
		put32(1);
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
		if (!in || std::string(magic, 4) != "CGVE" || !get32(version) || version != 1 || !get32(count)) return false;
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
		for (int py = 0; py < height + 2 * border; ++py) {
			const int y = py - border;
			if (y < 0 || y >= height) continue;
			for (int pz = 0; pz < p; ++pz) {
				const int lz = pz - border;
				const int nz = lz < 0 ? -1 : (lz >= size ? 1 : 0);
				const int inZ = lz - nz * size;
				for (int px = 0; px < p; ++px) {
					const int lx = px - border;
					const int nx = lx < 0 ? -1 : (lx >= size ? 1 : 0);
					const int inX = lx - nx * size;
					const Chunk* source = around[nz + 1][nx + 1];
					if (!source) continue;
					padded.blocks[static_cast<size_t>((py * p + pz) * p + px)] = source->blocks[localIndex(inX, y, inZ)];
				}
			}
		}
		return padded;
	}

	void VoxelWorld::startMeshing(int cx, int cz, Chunk& chunk) {
		chunk.dirty = false;
		chunk.meshing = true;
		dirtyKeys.erase(std::make_pair(cx, cz));
		const int resolution = chunk.resolution;
		PaddedChunk padded = snapshot(cx, cz, resolution > 0 ? 3 : 1);
		// Skirts on the sides that meet a neighbour at another level of detail (bit 1: -x, 2: +x, 4: -z, 8: +z).
		unsigned skirtSides = 0;
		if (resolution > 0) {
			const int dxs[4] = { -1, 1, 0, 0 };
			const int dzs[4] = { 0, 0, -1, 1 };
			for (int side = 0; side < 4; ++side) {
				const Chunk* neighbour = findChunk(cx + dxs[side], cz + dzs[side]);
				if (neighbour && neighbour->loaded && neighbour->resolution != resolution) skirtSides |= 1u << side;
			}
		}
		const std::array<glm::vec3, 256> colours = palette;
		const uint32_t generation = chunk.generation;
		const uint32_t version = chunk.version;
		enqueue([this, cx, cz, generation, version, resolution, skirtSides, padded = std::move(padded), colours]() mutable {
			MeshResult result;
			result.cx = cx;
			result.cz = cz;
			result.generation = generation;
			result.version = version;
			if (resolution > 0) {
				buildSmoothMesh(padded, colours, resolution, skirtSides, result.mesh);
			} else {
				buildChunkMesh(padded, colours, result.mesh);
			}
			result.quads = static_cast<int>(result.mesh.indices.size() / 6);
			std::lock_guard<std::mutex> lock(resultMutex);
			meshResults.push_back(std::move(result));
		});
	}

	std::string VoxelWorld::bodyName(int cx, int cz) const {
		return "voxel_chunk_" + std::to_string(cx) + "_" + std::to_string(cz);
	}

	void VoxelWorld::uploadMesh(MeshResult& result) {
		auto it = chunks.find(std::make_pair(result.cx, result.cz));
		if (it == chunks.end() || it->second.generation != result.generation) return;
		Chunk& chunk = it->second;
		chunk.meshing = false;
		// The chunk changed while it was being meshed. It is dirty again, so the next update meshes it again.
		if (result.version != chunk.version) return;

		std::vector<id_t> materials;
		if (auto materialId = assets.getId<Material>(material)) {
			materials.push_back(materialId.value());
		} else {
			materials.push_back(assets.getDefaultId<Material>().value_or(0));
		}
		Body* body = chunk.body.first ? assets.get<Body>(chunk.body.second) : nullptr;
		if (body) {
			updateCompactMeshBody(body, std::move(result.mesh.vertices), std::move(result.mesh.indices));
		} else if (!result.mesh.vertices.empty()) {
			const Vector3f position(static_cast<float>(result.cx * size), 0.0f, static_cast<float>(result.cz * size));
			body = createCompactMeshBody(bodyName(result.cx, result.cz), std::move(result.mesh.vertices),
				std::move(result.mesh.indices), materials, position);
			if (body) chunk.body = { true, body->getId().value_or(0) };
		}
		chunk.quads = result.quads;
	}

	int VoxelWorld::update(double budgetMs, int focusX, int focusZ) {
		refreshResolutions(focusX, focusZ, false);
		updateMovingBlocks();
		// 1. Install the terrain that finished generating.
		std::deque<TerrainResult> installed;
		{
			std::lock_guard<std::mutex> lock(resultMutex);
			installed.swap(terrainResults);
		}
		for (TerrainResult& result : installed) installTerrain(result);

		// 2. Upload finished meshes, until the budget is spent. At least one, so the queue always moves.
		const auto start = std::chrono::steady_clock::now();
		int uploaded = 0;
		for (;;) {
			MeshResult result;
			{
				std::lock_guard<std::mutex> lock(resultMutex);
				if (meshResults.empty()) break;
				const double spent = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
				if (uploaded > 0 && spent >= budgetMs) break;
				result = std::move(meshResults.front());
				meshResults.pop_front();
			}
			uploadMesh(result);
			++uploaded;
		}
		uploadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

		// 3. Start meshing the dirty chunks nearest the focus first.
		std::vector<std::pair<int, std::pair<int, int>>> candidates;
		for (const auto& key : dirtyKeys) {
			const Chunk* chunk = findChunk(key.first, key.second);
			if (!chunk || !chunk->loaded || !chunk->dirty || chunk->meshing) continue;
			const int dx = key.first - focusX;
			const int dz = key.second - focusZ;
			candidates.push_back({ dx * dx + dz * dz, key });
		}
		std::sort(candidates.begin(), candidates.end());
		const size_t starts = std::min(candidates.size(), static_cast<size_t>(MaxMeshStartsPerUpdate));
		for (size_t i = 0; i < starts; ++i) {
			const auto key = candidates[i].second;
			startMeshing(key.first, key.second, chunks.at(key));
		}
		return uploaded;
	}

	int VoxelWorld::loadedCount() const {
		int count = 0;
		for (const auto& entry : chunks) {
			if (entry.second.loaded) ++count;
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
		for (const auto& entry : chunks) {
			if (entry.second.meshing) ++count;
		}
		return count;
	}

	int VoxelWorld::quadCount() const {
		int count = 0;
		for (const auto& entry : chunks) count += entry.second.quads;
		return count;
	}

	void VoxelWorld::clear() {
		for (auto& entry : chunks) {
			if (entry.second.body.first) {
				if (Body* body = assets.get<Body>(entry.second.body.second)) removeRuntimeMesh(body);
			}
		}
		chunks.clear();
		dirtyKeys.clear();
	}

	// The blocks in flight are drawn as balls that move with them, in one runtime mesh that is rebuilt while any move.
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
