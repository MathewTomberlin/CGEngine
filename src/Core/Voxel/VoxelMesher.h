#pragma once

#include <array>
#include <bitset>
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

	// How bright a light block is: its glow is this many times its colour, its light in a cave is VoxelLampLight (below), and
	// the open gets less; see lampGain. The glow is clamped only where it is shown.
	constexpr float VoxelLampBrightness = 4.0f;

	// How much a light block's light is multiplied in a cave, where the sky gives none (see lampGain in VoxelMesher.cpp). In
	// the open it is 1, so sunlit ground is not washed out.
	constexpr float VoxelLampLight = 2.0f;

	// The light a torch gives at its own cell (0 to 1). A torch is a prop, not a block: it lights the air around it the way a
	// light block does, but less.
	constexpr float VoxelTorchLight = 0.8f;

	// The direction from a prop to the block it leans on, by its dir (VoxelWorld::setProp): +x, -x, +y, -y, +z, -z.
	constexpr int PropDirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

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
		// How each block id is drawn: the mesh it goes in (group 0 is the terrain, else the block's own id) and whether it is
		// cubes. Set from the world's block types (BlockType::own and cube). A mesh holds only the blocks of its group.
		struct MeshRule {
			uint8_t group = 0;
			bool cube = false;
			uint8_t join = 0;   // the mesh this block joins when the block above is in it (BlockType::joinAbove), else 0
			std::bitset<256> joinBelow;   // the blocks below that make this block join the terrain (BlockType::joinBelow)
		};
		std::array<MeshRule, 256> rule{};
		// The props (torches) in the chunk and its border, in chunk coordinates. A prop's cell is air: it gives light there, and
		// the terrain mesh draws it (buildPropMesh), but it is not a block, so collision and the faces around it do not change.
		struct Prop {
			int x, y, z;
			uint8_t dir;
		};
		std::vector<Prop> props;

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

	// Meshes the cube faces of one group of a chunk: group 0 is the terrain, else a block id's own mesh (PaddedChunk::rule).
	// A face is drawn only where the block beside it is not in the group, so a face against another group's block is drawn
	// here too. Faces that lie in one plane, with the same block, shade and occlusion, are merged into one quad (greedy
	// meshing), so flat ground is a few quads rather than one per block. Each vertex colour is the block colour times the
	// face shade times ambient occlusion.
	void buildChunkMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, uint8_t group, MeshArrays& out);

	// A smooth skin over one group of the blocks (see buildChunkMesh): a surface through the group's solid blocks, rounded
	// at the edges and corners. Surface nets (one vertex per cell the surface crosses, quads across the crossed edges) on
	// a density that is the group's occupancy, interpolated between block centres. resolution is the number of samples per
	// block: 1 is the block scale, 2 gives half-block detail. Chunks join without cracks, since every vertex is a function
	// of the blocks around it. The chunk needs a border of 3 (PaddedChunk::border).
	void buildSmoothMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, int resolution,
		unsigned skirtSides, uint8_t group, MeshArrays& out);

	// A round ball of a block's colour, centred at centre and appended to out. The moving blocks are drawn with it.
	void buildBlob(const std::array<glm::vec3, 256>& palette, uint8_t block, glm::vec3 centre, float radius, MeshArrays& out);

	// The geometry of the chunk's props (torches): a stick leaning on its block and a flame on top, for the props in the chunk
	// itself (not its border, which the neighbour draws). Appended to out, which is the terrain mesh.
	void buildPropMesh(const PaddedChunk& chunk, MeshArrays& out);
}
