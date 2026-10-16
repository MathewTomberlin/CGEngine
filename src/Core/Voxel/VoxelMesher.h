#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <vector>
#include "../World/Renderer.h"

namespace CGEngine {
	// The skin is the surface where the density is this (see buildSmoothMesh). Collision uses the same surface, so the drawn
	// ground and the ground a body stands on are one. The other smooth skins' density is their occupancy, interpolated between
	// block centres. The terrain's is the larger of a smoothing over the corners of the blocks (VoxelTerrainShift) and the blocks'
	// centres (VoxelCentreWeight), so a thin feature keeps its width.
	constexpr float VoxelSkinIso = 0.3f;

	// The terrain's density at a block corner is the share of the 2 by 2 by 2 blocks that meet there that are terrain, less this,
	// and it is interpolated between the corners. A flat face has half its blocks solid at its corners, so its density is 0.35 and
	// the surface sits 0.1 of a block out from the face. A wall one block thick keeps 0.35 inside it, so it stays solid, which a
	// wider smoothing would thin away. A slope of one block in one becomes a straight slope, not a step.
	constexpr float VoxelTerrainShift = 0.15f;

	// The terrain's density also takes the occupancy of the block centres, interpolated between them, and keeps the larger of the
	// two. The corner share alone thins a feature one block across: a column's corners hold only 2 of their 8 blocks, so the column
	// never reaches the iso, and a two-block tower loses its top. The centre term keeps such a feature at its true width: a
	// solid block's centre is 0.6, so the surface sits exactly half way between centres, on the block faces.
	constexpr float VoxelCentreWeight = 0.6f;

	// How far light and shade reach through air, in blocks. A cell's light depends only on blocks this close to it, so a
	// chunk with this much border computes the same light for a shared cell as its neighbour does.
	constexpr int VoxelLightReach = 8;

	// The glow of a light block or a torch: its light colour times this, added after the lighting. It stays under 1 in each
	// channel, so a glowing face keeps its warm colour instead of clipping to white (the held torch is lit the same way).
	constexpr float VoxelGlow = 0.8f;
	// The colour of the light the blocks give (the held torch's colour). The light a block gives its neighbours is tinted by it,
	// so a wall near a lamp is lit by a warm light, as the held torch lights it.
	const glm::vec3 VoxelLightTint(1.0f, 0.72f, 0.4f);

	// How much a light block's light is multiplied in a cave, where the sky gives none (see lampGain in VoxelMesher.cpp). In
	// the open it is 1, so sunlit ground is not washed out.
	constexpr float VoxelLampLight = 2.0f;

	// The light a torch gives at its own cell (0 to 1). A torch is a prop, not a block: it lights the air around it the way a
	// light block does, but less.
	constexpr float VoxelTorchLight = 0.8f;

	// How dark a block gets at its last damage stage (0 to 1 of its colour is taken away): a damaged block is shown darker as it
	// takes hits, so the player sees which blocks have been hit (VoxelWorld::hitBlock).
	constexpr float DamageTint = 0.45f;

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
			bool textured = false;        // drawn with the tile array (BlockType::textured)
			uint8_t tiles[3] = { 0, 0, 0 };   // its tile layers: top, sides, bottom (BlockType::tiles)
			int8_t splat = -1;                // its terrain splat channel, 0 to 7, or -1 (BlockType::splat)
			bool blend = false;               // its mesh is blended, drawn after the opaque ones (BlockType::blend)
			bool passesLight = false;         // light goes through it (BlockType::passesLight)
		};
		std::array<MeshRule, 256> rule{};
		// The props (torches) in the chunk and its border, in chunk coordinates. A prop's cell is air: it gives light there, and
		// the terrain mesh draws it (buildPropMesh), but it is not a block, so collision and the faces around it do not change.
		struct Prop {
			int x, y, z;
			uint8_t dir;
			float anchor = 0.5f;   // how far from the cell's centre, toward the block it leans on, the drawn skin is (VoxelWorld::propAnchor)
			glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);   // the skin's outward normal where the torch stands on it (VoxelWorld::propNormal)
		};
		std::vector<Prop> props;
		// The damage of each block position, in the layout of blocks: the fraction of its hits taken, 0 to 255. Empty if no block
		// in the padded chunk is damaged.
		std::vector<uint8_t> damage;
		// The terrain's density at each block corner, in the layout of blocks: the share of the 2 by 2 by 2 blocks meeting there that are
		// terrain, less VoxelTerrainShift (computeSkin). Empty until computeSkin runs; only the terrain's smooth mesh reads it.
		std::vector<float> skin;
		// The terrain's occupancy, 1 where a block is group 0 and 0 elsewhere, in the layout of blocks. Set by computeSkin with the
		// skin; the terrain's density reads the block centres from it, which is much cheaper than asking inGroup for each.
		std::vector<uint8_t> terrain;
		// The mesh group of each block position, in the layout of blocks (-1 for air). Set by computeSkin with the terrain; a smooth
		// mesh reads group membership from it, which is one lookup where inGroup works through the block's join rules.
		std::vector<int16_t> member;

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

	// The density of a group at p (block units), as the smooth mesh samples it (VoxelMesher.cpp densityAt).
	float densityAtPoint(const PaddedChunk& chunk, uint8_t group, glm::vec3 p);

	// Fills chunk.skin (see VoxelTerrainShift). The chunk's border must be at least 1, and the mesh samples two blocks beyond it.
	void computeSkin(PaddedChunk& chunk);

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

	// The block bounds of each group in a padded chunk, in the chunk's coordinates, inclusive on each axis (x, y, z). A group with no
	// block has lo > hi. One pass over the chunk, so it is taken once per chunk, not per group. Needs computeSkin (PaddedChunk::member).
	struct GroupBounds {
		int lo[3] = { 1 << 30, 1 << 30, 1 << 30 };
		int hi[3] = { -(1 << 30), -(1 << 30), -(1 << 30) };
	};
	std::array<GroupBounds, 256> groupBounds(const PaddedChunk& chunk);

	// A smooth skin over one group of the blocks (see buildChunkMesh): a surface through the group's solid blocks, rounded
	// at the edges and corners. Surface nets (one vertex per cell the surface crosses, quads across the crossed edges) on
	// a density that is the group's occupancy, interpolated between block centres. resolution is the number of samples per
	// block: 1 is the block scale, 2 gives half-block detail. Chunks join without cracks, since every vertex is a function
	// of the blocks around it. The chunk needs a border of 3 (PaddedChunk::border).
	// bounds is the group's blocks (groupBounds): the sample grid is only filled where they can reach, for a group of blocks.
	void buildSmoothMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, int resolution, int stride,
		unsigned skirtSides, uint8_t group, const GroupBounds& bounds, MeshArrays& out);

	// A round ball of a block's colour, centred at centre and appended to out. The moving blocks are drawn with it.
	void buildBlob(const std::array<glm::vec3, 256>& palette, uint8_t block, glm::vec3 centre, float radius, MeshArrays& out);

	// The boxes of a torch: its stick (centre and half size) and its flame. A torch stands on the skin of the block it leans on,
	// at distance anchor from its cell's centre, so the stick is outside the drawn surface and not hidden in it.
	struct PropShape {
		glm::vec3 stickCentre, stickHalf;   // the stick's centre, and the half size of the box round it (what the aim test uses)
		glm::vec3 stickAxes[3];             // the stick's own axes, and its half sizes along them (stickSize)
		glm::vec3 stickSize;
		glm::vec3 flameCentre, flameHalf;
	};
	// The stick stands out of the skin along its normal, with its foot on the skin at anchor toward the block it leans on.
	PropShape propShape(glm::vec3 cell, int dir, float anchor, glm::vec3 normal);

	// The geometry of the chunk's props (torches): a stick leaning on its block and a flame on top, for the props in the chunk
	// itself (not its border, which the neighbour draws). Appended to out, which is the terrain mesh.
	void buildPropMesh(const PaddedChunk& chunk, MeshArrays& out);
}
