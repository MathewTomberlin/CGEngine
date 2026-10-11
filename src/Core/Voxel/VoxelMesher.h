#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "../World/Renderer.h"

namespace CGEngine {
	// A chunk's blocks with a one-block border copied from its neighbours (air where a neighbour is not loaded). The
	// mesher reads only from this, so it can run on any thread while the world changes.
	struct PaddedChunk {
		int size = 16;
		int height = 64;
		// (height + 2) * (size + 2)^2 blocks. Block (x, y, z) of the chunk, with x and z in [-1, size] and y in
		// [-1, height], is at ((y + 1) * P + (z + 1)) * P + (x + 1) where P = size + 2.
		std::vector<uint8_t> blocks;

		uint8_t at(int x, int y, int z) const {
			const int p = size + 2;
			return blocks[static_cast<size_t>(((y + 1) * p + (z + 1)) * p + (x + 1))];
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
}
