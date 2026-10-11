#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "../World/Renderer.h"

namespace CGEngine {
	// A world of blocks split into square chunks. Each loaded chunk is a body with a runtime mesh (see RuntimeMesh.h),
	// rebuilt when it is dirty. A block change marks its chunk dirty, and the neighbours it touches (their faces
	// depend on it). Block 0 is air. Each block id has a colour (setColour). Faces are shaded by direction, and by
	// ambient occlusion: a corner next to solid blocks is darker. Chunk (cx, cz) covers x in [cx * size, cx * size + size)
	// and z likewise; its body sits at that corner, so its vertices are local to the chunk.
	class VoxelWorld {
	public:
		VoxelWorld(int chunkSize, int chunkHeight, const std::string& materialName);

		int chunkSize() const { return size; }
		int chunkHeight() const { return height; }

		void setColour(int blockId, glm::vec3 colour);
		int getBlock(int x, int y, int z) const;
		void setBlock(int x, int y, int z, int blockId);
		// Sets the blocks from yFrom (inclusive) to yTo (exclusive), clipped to the world height.
		void fillColumn(int x, int z, int yFrom, int yTo, int blockId);

		// Creates an empty chunk. False if it is loaded already.
		bool loadChunk(int cx, int cz);
		// Removes a chunk and its body. The neighbours are re-meshed, since their faces against it now show.
		void unloadChunk(int cx, int cz);
		bool isLoaded(int cx, int cz) const;
		// Re-meshes up to maxChunks dirty chunks. Returns how many were meshed.
		int rebuildDirty(int maxChunks);
		int dirtyCount() const;
		int chunkCount() const { return static_cast<int>(chunks.size()); }
		// Quads in the meshes of the loaded chunks, as last built.
		int quadCount() const;
		// Removes every chunk and its body.
		void clear();

	private:
		struct Chunk {
			std::vector<uint8_t> blocks;
			bool dirty = true;
			bool hasBody = false;
			int quads = 0;
		};

		int size;
		int height;
		std::string material;
		std::array<glm::vec3, 256> palette;
		std::map<std::pair<int, int>, Chunk> chunks;

		static int floorDiv(int a, int b);
		int localIndex(int lx, int y, int lz) const { return (y * size + lz) * size + lx; }
		const Chunk* findChunk(int cx, int cz) const;
		Chunk* findChunk(int cx, int cz);
		void markDirty(int cx, int cz);
		void markNeighboursDirty(int cx, int cz);
		void buildChunk(int cx, int cz, Chunk& chunk);
		std::string bodyName(int cx, int cz) const;
		bool solid(int x, int y, int z) const { return getBlock(x, y, z) != 0; }
	};
}
