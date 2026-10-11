#include <algorithm>
#include <array>
#include "VoxelWorld.h"
#include "../Engine/Engine.h"
#include "../Mesh/RuntimeMesh.h"

namespace CGEngine {
	namespace {
		// Brightness of a face by its direction: top faces are full strength, sides darker, the bottom darkest.
		float faceShade(int axis, int sign) {
			if (axis == 1) return sign > 0 ? 1.0f : 0.5f;
			if (axis == 0) return 0.8f;
			return 0.65f;
		}

		// Brightness of a face corner by its ambient occlusion level (0 = walled in, 3 = open).
		const float occlusionLight[4] = { 0.5f, 0.66f, 0.82f, 1.0f };

		// For a face whose normal is on axis 0, 1 or 2, the two axes that lie in the face.
		const int faceAxes[3][2] = { { 1, 2 }, { 2, 0 }, { 0, 1 } };
	}

	VoxelWorld::VoxelWorld(int chunkSize, int chunkHeight, const std::string& materialName)
		: size(chunkSize), height(chunkHeight), material(materialName) {
		palette.fill(glm::vec3(1.0f));
	}

	int VoxelWorld::floorDiv(int a, int b) {
		int q = a / b;
		if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
		return q;
	}

	void VoxelWorld::setColour(int blockId, glm::vec3 colour) {
		palette[static_cast<size_t>(std::clamp(blockId, 0, 255))] = colour;
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
		if (!chunk) return 0;	// blocks in chunks that are not loaded count as air
		return chunk->blocks[localIndex(x - cx * size, y, z - cz * size)];
	}

	void VoxelWorld::setBlock(int x, int y, int z, int blockId) {
		if (y < 0 || y >= height) return;
		const int cx = floorDiv(x, size);
		const int cz = floorDiv(z, size);
		Chunk* chunk = findChunk(cx, cz);
		if (!chunk) return;
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

	bool VoxelWorld::loadChunk(int cx, int cz) {
		const auto key = std::make_pair(cx, cz);
		if (chunks.count(key)) return false;
		Chunk& chunk = chunks[key];
		chunk.blocks.assign(static_cast<size_t>(size) * size * height, 0);
		chunk.dirty = true;
		markNeighboursDirty(cx, cz);
		return true;
	}

	void VoxelWorld::unloadChunk(int cx, int cz) {
		auto it = chunks.find(std::make_pair(cx, cz));
		if (it == chunks.end()) return;
		if (it->second.hasBody) removeRuntimeMeshBody(bodyName(cx, cz));
		chunks.erase(it);
		markNeighboursDirty(cx, cz);
	}

	bool VoxelWorld::isLoaded(int cx, int cz) const {
		return findChunk(cx, cz) != nullptr;
	}

	void VoxelWorld::markDirty(int cx, int cz) {
		if (Chunk* chunk = findChunk(cx, cz)) chunk->dirty = true;
	}

	void VoxelWorld::markNeighboursDirty(int cx, int cz) {
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dz = -1; dz <= 1; ++dz) {
				if (dx != 0 || dz != 0) markDirty(cx + dx, cz + dz);
			}
		}
	}

	int VoxelWorld::dirtyCount() const {
		int count = 0;
		for (const auto& entry : chunks) {
			if (entry.second.dirty) ++count;
		}
		return count;
	}

	int VoxelWorld::quadCount() const {
		int count = 0;
		for (const auto& entry : chunks) count += entry.second.quads;
		return count;
	}

	int VoxelWorld::rebuildDirty(int maxChunks) {
		int built = 0;
		for (auto& entry : chunks) {
			if (built >= maxChunks) break;
			if (!entry.second.dirty) continue;
			buildChunk(entry.first.first, entry.first.second, entry.second);
			entry.second.dirty = false;
			++built;
		}
		return built;
	}

	std::string VoxelWorld::bodyName(int cx, int cz) const {
		return "voxel_chunk_" + std::to_string(cx) + "_" + std::to_string(cz);
	}

	void VoxelWorld::buildChunk(int cx, int cz, Chunk& chunk) {
		std::vector<VertexData> vertices;
		std::vector<unsigned int> indices;
		const int originX = cx * size;
		const int originZ = cz * size;

		for (int y = 0; y < height; ++y) {
			for (int lz = 0; lz < size; ++lz) {
				for (int lx = 0; lx < size; ++lx) {
					const int blockId = chunk.blocks[localIndex(lx, y, lz)];
					if (blockId == 0) continue;
					const int wx = originX + lx;
					const int wz = originZ + lz;

					for (int axis = 0; axis < 3; ++axis) {
						for (int sign = -1; sign <= 1; sign += 2) {
							// The air cell this face looks into. Faces against solid blocks are hidden.
							std::array<int, 3> air = { wx, y, wz };
							air[axis] += sign;
							if (solid(air[0], air[1], air[2])) continue;

							const int u = faceAxes[axis][0];
							const int v = faceAxes[axis][1];
							// Corners go around the face: (0,0), (1,0), (1,1), (0,1) in the face's (u, v) axes.
							const int cornerU[4] = { 0, 1, 1, 0 };
							const int cornerV[4] = { 0, 0, 1, 1 };
							int occlusion[4];
							for (int i = 0; i < 4; ++i) {
								// The blocks beside a corner, in the air layer in front of the face.
								const int su = cornerU[i] ? 1 : -1;
								const int sv = cornerV[i] ? 1 : -1;
								std::array<int, 3> side1 = air, side2 = air, corner = air;
								side1[u] += su;
								side2[v] += sv;
								corner[u] += su;
								corner[v] += sv;
								const bool s1 = solid(side1[0], side1[1], side1[2]);
								const bool s2 = solid(side2[0], side2[1], side2[2]);
								const bool sc = solid(corner[0], corner[1], corner[2]);
								occlusion[i] = (s1 && s2) ? 0 : 3 - (s1 + s2 + sc);
							}

							const glm::vec3 base(static_cast<float>(lx), static_cast<float>(y), static_cast<float>(lz));
							glm::vec3 normal(0.0f);
							normal[axis] = static_cast<float>(sign);
							const unsigned int first = static_cast<unsigned int>(vertices.size());
							for (int i = 0; i < 4; ++i) {
								VertexData vertex;
								vertex.position = base;
								if (sign > 0) vertex.position[axis] += 1.0f;
								vertex.position[u] += static_cast<float>(cornerU[i]);
								vertex.position[v] += static_cast<float>(cornerV[i]);
								vertex.normal = normal;
								vertex.texCoord = glm::vec2(static_cast<float>(cornerU[i]), static_cast<float>(cornerV[i]));
								vertex.colour = palette[static_cast<size_t>(blockId)] * faceShade(axis, sign) * occlusionLight[occlusion[i]];
								vertices.push_back(vertex);
							}
							// Split the quad along the diagonal whose corners are less occluded, so shading does not streak.
							if (occlusion[0] + occlusion[2] > occlusion[1] + occlusion[3]) {
								indices.insert(indices.end(), { first + 1, first + 2, first + 3, first + 1, first + 3, first });
							} else {
								indices.insert(indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3 });
							}
						}
					}
				}
			}
		}

		std::vector<id_t> materials;
		if (auto materialId = assets.getId<Material>(material)) {
			materials.push_back(materialId.value());
		} else {
			materials.push_back(assets.getDefaultId<Material>().value_or(0));
		}
		const Vector3f position(static_cast<float>(originX), 0.0f, static_cast<float>(originZ));
		chunk.quads = static_cast<int>(indices.size() / 6);
		if (!chunk.hasBody) {
			if (vertices.empty()) return;	// nothing to show yet; the body is made once there is a face
			chunk.hasBody = createRuntimeMeshBody(bodyName(cx, cz), std::move(vertices), std::move(indices), materials, position) != nullptr;
		} else {
			updateRuntimeMeshBody(findBodyByName(bodyName(cx, cz)), std::move(vertices), std::move(indices));
		}
	}

	void VoxelWorld::clear() {
		for (auto& entry : chunks) {
			if (entry.second.hasBody) removeRuntimeMeshBody(bodyName(entry.first.first, entry.first.second));
		}
		chunks.clear();
	}
}
