#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "../World/Renderer.h"

namespace CGEngine {
	// The skin is the surface where the blocks' occupancy, interpolated between block centres, is this (see buildSmoothMesh).
	// Collision uses the same surface, so the drawn ground and the ground a body stands on are one.
	constexpr float VoxelSkinIso = 0.3f;

	// How far light and shade reach through air, in blocks. A cell's light depends only on blocks this close to it, so a
	// chunk with this much border computes the same light for a shared cell as its neighbour does.
	constexpr int VoxelLightReach = 8;

	// How bright a light block is: its glow is this many times its colour, and in a cave its light is too (the mesh scales the
	// light by less in the open, where the sky is already full; see lampGain). The glow is clamped only where it is shown.
	constexpr float VoxelLampBrightness = 4.0f;

	// A chunk's blocks with a border of blocks copied from its neighbours (air where a neighbour is not loaded). The
	// mesher reads only from this, so it can run on any thread while the world changes. The block mesher needs a border
	// of 1; the smooth mesher needs 3, because its density samples and their normals reach two blocks beyond the chunk.
	// Light needs VoxelLightReach, so the world builds every snapshot with that border.
	struct PaddedChunk {
		int size = 16;
		int height = 64;
		int border = 1;
		// (height + 2 * border) * (size + 2 * border)^2 blocks. Block (x, y, z) of the chunk, with x and z in
		// [-border, size + border) and y in [-border, height + border), is at ((y + border) * P + (z + border)) * P + (x + border)
		// where P = size + 2 * border.
		std::vector<uint8_t> blocks;
		// Sky light for each block position, in the same layout: 255 where the sky reaches, falling off through air (see
		// computeLight). Empty until computeLight runs.
		std::vector<uint8_t> light;
		// Light from the light blocks alone, in the same layout (0 where none reaches). The mesh adds it to the sky light, so a
		// lamp brightens a sunlit wall as well as a cave. The light at a block (VoxelWorld::lightAt) is the brighter of the two.
		std::vector<uint8_t> lamp;
		// The light each block id gives off at its own cell, 0 to 255. Set from the world's block types (BlockType::emits).
		std::array<uint8_t, 256> emission{};

		size_t index(int x, int y, int z) const {
			const int p = size + 2 * border;
			return static_cast<size_t>(((y + border) * p + (z + border)) * p + (x + border));
		}

		uint8_t at(int x, int y, int z) const {
			return blocks[index(x, y, z)];
		}

		uint8_t lightAt(int x, int y, int z) const {
			return light[index(x, y, z)];
		}

		uint8_t lampAt(int x, int y, int z) const {
			return lamp[index(x, y, z)];
		}
	};

	// Fills chunk.light. A block is open to the sky if no block is above it in its column; the light of open air is full,
	// and each block walked through air from there multiplies it by 0.8, and nothing beyond VoxelLightReach steps, so a sealed cave has no sky light at any hour.
	// A light block (chunk.emission) is full at its own cell, and the light it gives multiplies by 0.8 for each block walked
	// through air, for VoxelLightReach blocks; it goes to chunk.lamp, apart from the sky's. Any block counts as opaque, the same
	// as in the meshes, so caves and the ground under trees are dark until a light block is in them.
	void computeLight(PaddedChunk& chunk);

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
	void buildSmoothMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, int resolution,
		unsigned skirtSides, MeshArrays& out);

	// A round ball of a block's colour, centred at centre and appended to out. The moving blocks are drawn with it.
	void buildBlob(const std::array<glm::vec3, 256>& palette, uint8_t block, glm::vec3 centre, float radius, MeshArrays& out);
}
