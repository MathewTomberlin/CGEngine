#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "../World/Renderer.h"

namespace CGEngine {
	// A chunk's blocks with a border of blocks copied from its neighbours (air where a neighbour is not loaded). The
	// mesher reads only from this, so it can run on any thread while the world changes. The block mesher needs a border
	// of 1; the smooth mesher needs 3, because its density samples and their normals reach two blocks beyond the chunk.
	struct PaddedChunk {
		int size = 16;
		int height = 64;
		int border = 1;
		// (height + 2 * border) * (size + 2 * border)^2 blocks. Block (x, y, z) of the chunk, with x and z in
		// [-border, size + border) and y in [-border, height + border), is at ((y + border) * P + (z + border)) * P + (x + border)
		// where P = size + 2 * border.
		std::vector<uint8_t> blocks;

		uint8_t at(int x, int y, int z) const {
			const int p = size + 2 * border;
			return blocks[static_cast<size_t>(((y + border) * p + (z + border)) * p + (x + border))];
		}
	};

	struct MeshArrays {
		std::vector<VoxelVertex> vertices;
		std::vector<unsigned int> indices;
	};

	// Meshes the solid faces of a chunk. A face is drawn only where the block beside it is air. Faces that lie in one
	// plane, with the same block, shade and occlusion, are merged into one quad (greedy meshing), so flat ground is a few
	// quads rather than one per block. Each vertex colour is the block colour times the face shade times ambient occlusion.
	void buildChunkMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, MeshArrays& out);

	// A smooth skin over the blocks: a surface through the solid blocks, rounded at the edges and corners, that the
	// chunk's own mesh is shaped to. Surface nets (one vertex per cell the surface crosses, quads across the crossed
	// edges) on a density that is the blocks' occupancy, interpolated between block centres. resolution is the number of
	// samples per block: 1 is the block scale, 2 gives half-block detail. Chunks join without cracks, since every vertex
	// is a function of the blocks around it. The chunk needs a border of 3 (PaddedChunk::border).
	void buildSmoothMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, int resolution, MeshArrays& out);

	// A round ball of a block's colour, centred at centre and appended to out. The moving blocks are drawn with it.
	void buildBlob(const std::array<glm::vec3, 256>& palette, uint8_t block, glm::vec3 centre, float radius, MeshArrays& out);
}
