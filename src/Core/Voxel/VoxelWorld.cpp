#include <algorithm>
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
		startWorkers();
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
		Chunk* chunk = findChunk(cx, cz);
		if (!chunk || !chunk->loaded) return;
		const int lx = x - cx * size;
		const int lz = z - cz * size;
		chunk->blocks[localIndex(lx, y, lz)] = static_cast<uint8_t>(std::clamp(blockId, 0, 255));
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
		}
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
		chunks.erase(it);
		markNeighboursDirty(cx, cz);
	}

	void VoxelWorld::installTerrain(TerrainResult& result) {
		auto it = chunks.find(std::make_pair(result.cx, result.cz));
		if (it == chunks.end() || it->second.generation != result.generation || it->second.loaded) return;
		Chunk& chunk = it->second;
		chunk.blocks = std::move(result.blocks);
		chunk.loaded = true;
		// The chunk's faces, and those of the neighbours that face it, are meshed from the new blocks.
		markDirty(result.cx, result.cz);
		markNeighboursDirty(result.cx, result.cz);
	}

	PaddedChunk VoxelWorld::snapshot(int cx, int cz) const {
		PaddedChunk padded;
		padded.size = size;
		padded.height = height;
		const int p = size + 2;
		padded.blocks.assign(static_cast<size_t>(p) * p * (height + 2), 0);

		// The nine chunks around (cx, cz), indexed by the offset of the chunk from it: [dz + 1][dx + 1].
		const Chunk* around[3][3];
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dx = -1; dx <= 1; ++dx) {
				const Chunk* chunk = findChunk(cx + dx, cz + dz);
				around[dz + 1][dx + 1] = chunk && chunk->loaded ? chunk : nullptr;
			}
		}
		for (int py = 0; py < height + 2; ++py) {
			const int y = py - 1;
			if (y < 0 || y >= height) continue;
			for (int pz = 0; pz < p; ++pz) {
				const int lz = pz - 1;
				const int nz = lz < 0 ? -1 : (lz >= size ? 1 : 0);
				const int inZ = lz - nz * size;
				for (int px = 0; px < p; ++px) {
					const int lx = px - 1;
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
		PaddedChunk padded = snapshot(cx, cz);
		const std::array<glm::vec3, 256> colours = palette;
		const uint32_t generation = chunk.generation;
		const uint32_t version = chunk.version;
		enqueue([this, cx, cz, generation, version, padded = std::move(padded), colours]() mutable {
			MeshResult result;
			result.cx = cx;
			result.cz = cz;
			result.generation = generation;
			result.version = version;
			buildChunkMesh(padded, colours, result.mesh);
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
		for (const auto& entry : chunks) {
			const Chunk& chunk = entry.second;
			if (!chunk.loaded || !chunk.dirty || chunk.meshing) continue;
			const int dx = entry.first.first - focusX;
			const int dz = entry.first.second - focusZ;
			candidates.push_back({ dx * dx + dz * dz, entry.first });
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
	}
}
