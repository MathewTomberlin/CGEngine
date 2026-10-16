#include <algorithm>
#include <cmath>
#include "VoxelMesher.h"
#include "VoxelBlocks.h"

namespace CGEngine {
	namespace {
		// A cell of the face mask for one plane: the block whose face is there (0 for none), which way it points,
		// and the occlusion of its four corners. Cells merge only when all of these match.
		struct MaskCell {
			uint8_t block = 0;
			int8_t sign = 0;
			uint8_t occlusion[4] = { 0, 0, 0, 0 };
			// The sky light of the air in front. Faces merge when it is close (within an eighth), and the quad takes this one.
			uint8_t light = 0;
			// The light of the lamps in the air in front (PaddedChunk::lamp). It is added to the sky light, and merges like it.
			uint8_t lamp = 0;
			// The light the block gives off (BlockType::emits), for the glow of its faces.
			uint8_t emission = 0;
			// The damage of the block (PaddedChunk::damage): faces of different damage do not merge.
			uint8_t damage = 0;
			// The tile layers of the face's block (top, sides, bottom; see tileLayers), and whether it is drawn with the tile array.
			glm::vec3 tiles = glm::vec3(255.0f);
			bool textured = false;

			bool operator==(const MaskCell& other) const {
				return block == other.block && damage == other.damage && sign == other.sign && (light >> 5) == (other.light >> 5) && (lamp >> 5) == (other.lamp >> 5)
					&& occlusion[0] == other.occlusion[0] && occlusion[1] == other.occlusion[1]
					&& occlusion[2] == other.occlusion[2] && occlusion[3] == other.occlusion[3];
			}
		};

		// How much a light block's light is multiplied where the sky is at sky (0 to 1): VoxelLampLight in a cave, and 1 in
		// the open, where the sun already lights the ground, so a lamp does not wash out sunlit grass.
		float lampGain(float sky) {
			return 1.0f + (VoxelLampLight - 1.0f) * (1.0f - sky);
		}

		// Brightness of a corner by its occlusion (0 = walled in, 3 = open).
		const float occlusionLight[4] = { 0.5f, 0.66f, 0.82f, 1.0f };

		// The tile layers of a block: its top, its sides and its bottom, which the fragment shader blends by each surface's normal.
		// A block that is not textured has none (255, NoTileLayer).
		glm::vec3 tileLayers(const PaddedChunk::MeshRule& rule) {
			if (!rule.textured) return glm::vec3(255.0f);
			return glm::vec3(rule.tiles[0], rule.tiles[1], rule.tiles[2]);
		}

		// Brightness of a face by its direction: top full strength, sides darker, bottom darkest.
		float faceShade(int axis, int sign) {
			if (axis == 1) return sign > 0 ? 1.0f : 0.5f;
			if (axis == 0) return 0.8f;
			return 0.65f;
		}

		// The corners of a face in its (u, v) axes, in order around the face.
		const int cornerU[4] = { 0, 1, 1, 0 };
		const int cornerV[4] = { 0, 0, 1, 1 };

		// The group of the block at (x, y, z), or -1 for air. Each block is in exactly one group: group 0 is the terrain, which holds
		// every block drawn in no group of its own.
		int memberOf(const PaddedChunk& chunk, int x, int y, int z) {
			const uint8_t block = chunk.at(x, y, z);
			if (block == 0) return -1;
			const PaddedChunk::MeshRule& rule = chunk.rule[block];
			// A block whose top is covered by its join mesh belongs to that mesh instead (the cover is the block above it).
			if (rule.join && y + 1 < chunk.height + chunk.border && chunk.rule[chunk.at(x, y + 1, z)].group == rule.join) {
				return rule.join;
			}
			// A block standing on natural ground joins the terrain (its base only).
			if (rule.joinBelow.any() && y - 1 >= -chunk.border && rule.joinBelow[chunk.at(x, y - 1, z)]) {
				return 0;
			}
			return rule.group;
		}

		// Whether the block at (x, y, z) is in a mesh group: a block whose group is this one (group 0 is the terrain, which holds
		// every block drawn in no group of its own). Air is in no group. Blocks of other groups count as air to this mesh.
		bool inGroup(const PaddedChunk& chunk, int x, int y, int z, uint8_t group) {
			return memberOf(chunk, x, y, z) == group;
		}

		// Occlusion of each corner of a face, from the air cell in front of it. Each corner has two cells beside it and
		// one diagonal, all in the air layer; two solid sides close the corner completely.
		void cornerOcclusion(const PaddedChunk& chunk, const int air[3], int u, int v, uint8_t group, uint8_t out[4]) {
			for (int k = 0; k < 4; ++k) {
				const int su = cornerU[k] ? 1 : -1;
				const int sv = cornerV[k] ? 1 : -1;
				int side1[3] = { air[0], air[1], air[2] };
				int side2[3] = { air[0], air[1], air[2] };
				int corner[3] = { air[0], air[1], air[2] };
				side1[u] += su;
				side2[v] += sv;
				corner[u] += su;
				corner[v] += sv;
				const bool s1 = inGroup(chunk, side1[0], side1[1], side1[2], group);
				const bool s2 = inGroup(chunk, side2[0], side2[1], side2[2], group);
				const bool sc = inGroup(chunk, corner[0], corner[1], corner[2], group);
				out[k] = (s1 && s2) ? 0 : static_cast<uint8_t>(3 - (s1 + s2 + sc));
			}
		}

		// One quad covering w by h cells of the mask, on plane s of axis d, with (u, v) its other two axes.
		void emitQuad(MeshArrays& out, const std::array<glm::vec3, 256>& palette, int d, int u, int v, int sign, int s,
			int i, int j, int w, int h, const MaskCell& face) {
			const unsigned int first = static_cast<unsigned int>(out.vertices.size());
			glm::vec3 normal(0.0f);
			normal[d] = static_cast<float>(sign);
			const glm::vec3 base = (face.textured ? glm::vec3(1.0f) : palette[face.block]) * faceShade(d, sign) * (1.0f - DamageTint * face.damage / 255.0f);
			for (int k = 0; k < 4; ++k) {
				VoxelVertex vertex;
				glm::vec3 position(0.0f);
				position[d] = static_cast<float>(s);
				position[u] = static_cast<float>(i + cornerU[k] * w);
				position[v] = static_cast<float>(j + cornerV[k] * h);
				vertex.position = position;
				vertex.normal = normal;
				// Texture repeats once per block across the quad, so a big quad tiles like its blocks would.
				vertex.texCoord = glm::vec2(static_cast<float>(cornerU[k] * w), static_cast<float>(cornerV[k] * h));
				vertex.colour = glm::vec4(base * occlusionLight[face.occlusion[k]], 1.0f);
				vertex.tiles = face.tiles;
				vertex.skyDamage.x = face.light / 255.0f;
				vertex.skyDamage.y = face.damage / 255.0f;
				vertex.glow = glm::vec4(VoxelLightTint * (face.emission / 255.0f * VoxelGlow)
					+ VoxelLightTint * (face.lamp / 255.0f * lampGain(face.light / 255.0f)), 0.0f);
				out.vertices.push_back(vertex);
			}
			// Split the quad along the diagonal between its less occluded corners, so the shading does not streak.
			if (face.occlusion[0] + face.occlusion[2] > face.occlusion[1] + face.occlusion[3]) {
				out.indices.insert(out.indices.end(), { first + 1, first + 2, first + 3, first + 1, first + 3, first });
			} else {
				out.indices.insert(out.indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3 });
			}
		}
	}

	namespace {
		// Light kept per block walked through air from the open sky, and the least a cell gets beyond that reach.
		const float LightPerStep = 0.8f;
		const float LightFloor = 0.0f;
	}

	// Whether light goes through a block: air, and the water (its mesh is blended, so the ground under it is lit as well).
	bool lightPasses(const PaddedChunk& chunk, uint8_t block) {
		return block == 0 || chunk.rule[block].passesLight;
	}

	void computeLight(PaddedChunk& chunk) {
		struct Cell {
			int x, y, z;
		};
		const int b = chunk.border;
		const int h = chunk.height;
		const int lo = -b;
		const int hi = chunk.size + b;
		// steps: the walk from the open sky to each air cell, 0xFF while it is not reached (air under a block, or too far).
		std::vector<uint8_t> steps(chunk.blocks.size(), 0xFF);
		std::vector<Cell> frontier;
		for (int z = lo; z < hi; ++z) {
			for (int x = lo; x < hi; ++x) {
				// Walking down the column, the air above the first block is open to the sky; the air under a block is not.
				bool open = true;
				for (int y = h - 1; y >= 0; --y) {
					if (!lightPasses(chunk, chunk.at(x, y, z))) {
						open = false;
						continue;
					}
					if (open) {
						steps[chunk.index(x, y, z)] = 0;
						frontier.push_back({ x, y, z });
					}
				}
			}
		}

		static const int Directions[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		std::vector<Cell> next;
		for (int step = 0; step < VoxelLightReach && !frontier.empty(); ++step) {
			next.clear();
			for (const Cell& cell : frontier) {
				for (const auto& d : Directions) {
					const int x = cell.x + d[0], y = cell.y + d[1], z = cell.z + d[2];
					if (x < lo || x >= hi || z < lo || z >= hi || y < 0 || y >= h) continue;
					const size_t i = chunk.index(x, y, z);
					if (!lightPasses(chunk, chunk.blocks[i]) || steps[i] != 0xFF) continue;
					steps[i] = static_cast<uint8_t>(step + 1);
					next.push_back({ x, y, z });
				}
			}
			frontier.swap(next);
		}

		// Light from the light blocks: full at the block, and each block walked through air (or any block that is not opaque)
		// multiplies it by LightPerStep, for VoxelLightReach rounds. A cell keeps the brightest that reaches it. Nothing is
		// done when the chunk has no light block, which is most of the time.
		std::vector<float> emitted;
		std::vector<Cell> sources;
		for (int z = lo; z < hi; ++z) {
			for (int x = lo; x < hi; ++x) {
				for (int y = 0; y < h; ++y) {
					const uint8_t level = chunk.emission[chunk.at(x, y, z)];
					if (level == 0) continue;
					if (emitted.empty()) emitted.assign(chunk.blocks.size(), 0.0f);
					emitted[chunk.index(x, y, z)] = level / 255.0f;
					sources.push_back({ x, y, z });
				}
			}
		}
		// Torches: a prop lights its own cell, which is air, so it joins the walk like a light block.
		for (const PaddedChunk::Prop& prop : chunk.props) {
			if (emitted.empty()) emitted.assign(chunk.blocks.size(), 0.0f);
			const size_t i = chunk.index(prop.x, prop.y, prop.z);
			if (emitted[i] >= VoxelTorchLight) continue;
			emitted[i] = VoxelTorchLight;
			sources.push_back({ prop.x, prop.y, prop.z });
		}
		for (int round = 0; round < VoxelLightReach && !sources.empty(); ++round) {
			next.clear();
			for (const Cell& cell : sources) {
				const float carried = emitted[chunk.index(cell.x, cell.y, cell.z)] * LightPerStep;
				for (const auto& d : Directions) {
					const int x = cell.x + d[0], y = cell.y + d[1], z = cell.z + d[2];
					if (x < lo || x >= hi || z < lo || z >= hi || y < 0 || y >= h) continue;
					const size_t i = chunk.index(x, y, z);
					if (chunk.blocks[i] != 0 && chunk.emission[chunk.blocks[i]] == 0) continue;
					if (carried <= emitted[i]) continue;
					emitted[i] = carried;
					next.push_back({ x, y, z });
				}
			}
			sources.swap(next);
		}

		float byStep[VoxelLightReach + 1];
		for (int s = 0; s <= VoxelLightReach; ++s) {
			byStep[s] = std::max(LightFloor, std::pow(LightPerStep, static_cast<float>(s)));
		}
		// Every position of the padded chunk, including the air above the world (full light) and below it (none). The levels are a
		// few fixed values, so each one's byte is worked out once (the same rounding as a per-cell lround).
		uint8_t stepByte[VoxelLightReach + 1];
		for (int s = 0; s <= VoxelLightReach; ++s) stepByte[s] = static_cast<uint8_t>(std::lround(byStep[s] * 255.0f));
		const uint8_t floorByte = static_cast<uint8_t>(std::lround(LightFloor * 255.0f));
		chunk.light.assign(chunk.blocks.size(), 0);
		chunk.lamp.assign(chunk.blocks.size(), 0);
		for (int z = lo; z < hi; ++z) {
			for (int x = lo; x < hi; ++x) {
				for (int y = -b; y < h + b; ++y) {
					const size_t i = chunk.index(x, y, z);
					if (y >= h) {
						chunk.light[i] = 255;
					} else if (y >= 0 && lightPasses(chunk, chunk.blocks[i])) {
						chunk.light[i] = steps[i] == 0xFF ? floorByte : stepByte[steps[i]];
					}
					if (!emitted.empty() && emitted[i] > 0.0f) chunk.lamp[i] = static_cast<uint8_t>(std::lround(emitted[i] * 255.0f));
				}
			}
		}
	}

	void computeSkin(PaddedChunk& chunk) {
		// The terrain's occupancy, 1 in a block of group 0 and 0 elsewhere, in the padded layout of PaddedChunk::index.
		const int b = chunk.border;
		const int P = chunk.size + 2 * b;
		const int Y = chunk.height + 2 * b;
		const size_t n = static_cast<size_t>(P) * P * Y;
		std::vector<int> solid(n, 0);
		chunk.terrain.assign(n, 0);
		chunk.member.assign(n, -1);
		for (int y = -b; y < chunk.height + b; ++y) {
			for (int z = -b; z < chunk.size + b; ++z) {
				for (int x = -b; x < chunk.size + b; ++x) {
					const int group = memberOf(chunk, x, y, z);
					chunk.member[chunk.index(x, y, z)] = static_cast<int16_t>(group);
					solid[chunk.index(x, y, z)] = group == 0 ? 1 : 0;
					chunk.terrain[chunk.index(x, y, z)] = static_cast<uint8_t>(solid[chunk.index(x, y, z)]);
				}
			}
		}
		// A corner's value sums the two blocks on each side of it along x, then z, then y: the 2 by 2 by 2 blocks that meet there.
		// The first block on the padded edge has no block before it, and nothing samples that far out.
		std::vector<int> along(n, 0);
		for (int y = 0; y < Y; ++y) {
			for (int z = 0; z < P; ++z) {
				for (int x = 0; x < P; ++x) {
					const size_t i = (static_cast<size_t>(y) * P + z) * P + x;
					along[i] = solid[i] + (x > 0 ? solid[i - 1] : 0);
				}
			}
		}
		for (int y = 0; y < Y; ++y) {
			for (int z = 0; z < P; ++z) {
				for (int x = 0; x < P; ++x) {
					const size_t i = (static_cast<size_t>(y) * P + z) * P + x;
					solid[i] = along[i] + (z > 0 ? along[i - P] : 0);
				}
			}
		}
		for (int y = 0; y < Y; ++y) {
			for (int z = 0; z < P; ++z) {
				for (int x = 0; x < P; ++x) {
					const size_t i = (static_cast<size_t>(y) * P + z) * P + x;
					along[i] = solid[i] + (y > 0 ? solid[i - static_cast<size_t>(P) * P] : 0);
				}
			}
		}
		chunk.skin.assign(n, 0.0f);
		for (size_t i = 0; i < n; ++i) {
			chunk.skin[i] = along[i] / 8.0f - VoxelTerrainShift;
		}
	}

	void buildChunkMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, uint8_t group, MeshArrays& out) {
		out.vertices.clear();
		out.indices.clear();
		const int dims[3] = { chunk.size, chunk.height, chunk.size };
		std::vector<MaskCell> mask;

		for (int d = 0; d < 3; ++d) {
			// u and v are the axes in the plane of the faces; d is their normal.
			const int u = (d + 1) % 3;
			const int v = (d + 2) % 3;
			const int du = dims[u];
			const int dv = dims[v];
			const int dd = dims[d];
			mask.assign(static_cast<size_t>(du) * dv, MaskCell());

			for (int sign = -1; sign <= 1; sign += 2) {
				// Plane s lies between cells s - 1 and s along d. Its faces point towards sign.
				for (int s = 0; s <= dd; ++s) {
					const int solidCoord = sign > 0 ? s - 1 : s;
					// Only blocks inside this chunk are drawn. The faces of a neighbour's blocks belong to its own mesh.
					if (solidCoord < 0 || solidCoord >= dd) continue;

					for (int j = 0; j < dv; ++j) {
						for (int i = 0; i < du; ++i) {
							int solid[3];
							solid[d] = solidCoord;
							solid[u] = i;
							solid[v] = j;
							int air[3] = { solid[0], solid[1], solid[2] };
							air[d] += sign;

							MaskCell& cell = mask[static_cast<size_t>(j) * du + i];
							cell = MaskCell();
							if (!inGroup(chunk, solid[0], solid[1], solid[2], group) || inGroup(chunk, air[0], air[1], air[2], group)) continue;
							const uint8_t block = chunk.at(solid[0], solid[1], solid[2]);
							cell.block = block;
							cell.emission = chunk.emission[block];
							cell.textured = chunk.rule[block].textured;
							cell.tiles = tileLayers(chunk.rule[block]);
							cell.damage = chunk.damage.empty() ? 0 : chunk.damage[chunk.index(solid[0], solid[1], solid[2])];
							cell.sign = static_cast<int8_t>(sign);
							cornerOcclusion(chunk, air, u, v, group, cell.occlusion);
							cell.light = chunk.lightAt(air[0], air[1], air[2]);
							cell.lamp = chunk.lampAt(air[0], air[1], air[2]);
						}
					}

					// Greedy merge: take the widest run along u, then extend it as far as the rows below match.
					for (int j = 0; j < dv; ++j) {
						for (int i = 0; i < du;) {
							const MaskCell start = mask[static_cast<size_t>(j) * du + i];
							if (start.block == 0) {
								++i;
								continue;
							}
							int w = 1;
							while (i + w < du && mask[static_cast<size_t>(j) * du + i + w] == start) ++w;
							int h = 1;
							bool more = true;
							while (more && j + h < dv) {
								for (int k = 0; k < w; ++k) {
									if (!(mask[static_cast<size_t>(j + h) * du + i + k] == start)) {
										more = false;
										break;
									}
								}
								if (more) ++h;
							}
							emitQuad(out, palette, d, u, v, sign, s, i, j, w, h, start);
							for (int dj = 0; dj < h; ++dj) {
								for (int di = 0; di < w; ++di) {
									mask[static_cast<size_t>(j + dj) * du + i + di] = MaskCell();
								}
							}
							i += w;
						}
					}
				}
			}
		}
	}

	namespace {
		// The surface is where the density crosses this: a sample inside the solid blocks is above it.
		const float SmoothIso = VoxelSkinIso;

		// The 12 edges of a cell's cube, as pairs of corner indices (corner bit 0 is x, bit 1 is y, bit 2 is z).
		const int CubeEdges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };

		// Whether the block at (x, y, z) is a surface block of the group: one of its six faces has a neighbour that is not in the group
		// (see smoothVertex). Only the group's own membership counts, so the ground under water is exposed (the water is not ground)
		// and keeps its skin, and light has no part in it.
		bool exposedBlock(const PaddedChunk& chunk, int x, int y, int z, uint8_t group) {
			static const int Faces[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
			for (const auto& f : Faces) {
				if (!inGroup(chunk, x + f[0], y + f[1], z + f[2], group)) return true;
			}
			return false;
		}

		// Whether the terrain (group 0) is in the block at (x, y, z), from PaddedChunk::terrain. Outside the padded chunk it is air.
		bool terrainAt(const PaddedChunk& chunk, int x, int y, int z) {
			const int b = chunk.border;
			if (x < -b || y < -b || z < -b || x >= chunk.size + b || y >= chunk.height + b || z >= chunk.size + b) return false;
			return chunk.terrain[chunk.index(x, y, z)] != 0;
		}

		// The block id at (x, y, z), 0 (air) outside the padded chunk.
		int blockAt(const PaddedChunk& chunk, int x, int y, int z) {
			const int b = chunk.border;
			if (x < -b || y < -b || z < -b || x >= chunk.size + b || y >= chunk.height + b || z >= chunk.size + b) return 0;
			return chunk.at(x, y, z);
		}

		// Whether the block at (x, y, z) is in the group. Outside the padded chunk it is air.
		bool solidAt(const PaddedChunk& chunk, uint8_t group, int x, int y, int z) {
			const int b = chunk.border;
			if (x < -b || y < -b || z < -b || x >= chunk.size + b || y >= chunk.height + b || z >= chunk.size + b) return false;
			return chunk.member[chunk.index(x, y, z)] == group;
		}

		// The density at p (block units). The terrain's is its smoothed occupancy, interpolated between block corners (VoxelTerrainShift),
		// or the occupancy of its block centres (VoxelCentreWeight), whichever is larger: a thin feature keeps its width, a broad one
		// keeps its smooth corners. The other groups' is their occupancy, interpolated between block centres, which are the
		// corner + 0.5, so it falls from 1 to 0 across each block face and the iso-surface runs along the faces.
		float densityAt(const PaddedChunk& chunk, uint8_t group, glm::vec3 p) {
			if (group == 0) {
				const glm::vec3 base = glm::floor(p);
				const glm::vec3 t = p - base;
				const int x0 = static_cast<int>(base.x), y0 = static_cast<int>(base.y), z0 = static_cast<int>(base.z);
				float corners = 0.0f;
				for (int c = 0; c < 8; ++c) {
					const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
					const float w = (dx ? t.x : 1.0f - t.x) * (dy ? t.y : 1.0f - t.y) * (dz ? t.z : 1.0f - t.z);
					corners += w * chunk.skin[chunk.index(x0 + dx, y0 + dy, z0 + dz)];
				}
				const glm::vec3 b = p - glm::vec3(0.5f);
				const glm::vec3 centreBase = glm::floor(b);
				const glm::vec3 u = b - centreBase;
				const int cx0 = static_cast<int>(centreBase.x), cy0 = static_cast<int>(centreBase.y), cz0 = static_cast<int>(centreBase.z);
				float centres = 0.0f;
				for (int c = 0; c < 8; ++c) {
					const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
					const float w = (dx ? u.x : 1.0f - u.x) * (dy ? u.y : 1.0f - u.y) * (dz ? u.z : 1.0f - u.z);
					if (w > 0.0f && terrainAt(chunk, cx0 + dx, cy0 + dy, cz0 + dz)) centres += w;
				}
				return std::max(corners, VoxelCentreWeight * centres);
			}
			const glm::vec3 b = p - glm::vec3(0.5f);
			const glm::vec3 base = glm::floor(b);
			const glm::vec3 t = b - base;
			const int x0 = static_cast<int>(base.x), y0 = static_cast<int>(base.y), z0 = static_cast<int>(base.z);
			float value = 0.0f;
			for (int c = 0; c < 8; ++c) {
				const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
				const float w = (dx ? t.x : 1.0f - t.x) * (dy ? t.y : 1.0f - t.y) * (dz ? t.z : 1.0f - t.z);
				if (solidAt(chunk, group, x0 + dx, y0 + dy, z0 + dz)) value += w;
			}
			return value;
		}

		// The splat channel of the nearest terrain block that has one, looking out a block from the point's cell. The dirt channel
		// is the fallback when there is none.
		int nearestSplatChannel(const PaddedChunk& chunk, glm::vec3 p) {
			const glm::vec3 base = glm::floor(p - glm::vec3(0.5f));
			int channel = 1;
			float nearest = 1e30f;
			for (int z = -1; z <= 2; ++z) {
				for (int y = -1; y <= 2; ++y) {
					for (int x = -1; x <= 2; ++x) {
						const int bx = static_cast<int>(base.x) + x, by = static_cast<int>(base.y) + y, bz = static_cast<int>(base.z) + z;
						if (!solidAt(chunk, 0, bx, by, bz)) continue;
						const int splat = chunk.rule[chunk.at(bx, by, bz)].splat;
						if (splat < 0) continue;
						const glm::vec3 centre(bx + 0.5f, by + 0.5f, bz + 0.5f);
						const float distance = glm::dot(centre - p, centre - p);
						if (distance < nearest) {
							nearest = distance;
							channel = splat;
						}
					}
				}
			}
			return channel;
		}

		// The vertex at p (chunk block units): a normal from the density gradient, the colour of the solid block that
		// weighs most there, darkened where the air beside it is walled in, and a texture coordinate from the normal's axis.
		VoxelVertex smoothVertex(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, uint8_t group, glm::vec3 p, int stride = 1) {
			const float h = 0.25f;
			const glm::vec3 gradient(
				densityAt(chunk, group,p + glm::vec3(h, 0, 0)) - densityAt(chunk, group,p - glm::vec3(h, 0, 0)),
				densityAt(chunk, group,p + glm::vec3(0, h, 0)) - densityAt(chunk, group,p - glm::vec3(0, h, 0)),
				densityAt(chunk, group,p + glm::vec3(0, 0, h)) - densityAt(chunk, group,p - glm::vec3(0, 0, h)));
			const float length = glm::length(gradient);
			const glm::vec3 normal = length > 1e-6f ? -gradient / length : glm::vec3(0.0f, 1.0f, 0.0f);

			// The dominant solid block: the one whose centre is nearest the point, among those with air beside them. A vertex on the
			// ground takes the colour of the surface block, not of the dirt under the grass, which a coarse vertex can be nearer to.
			// A block with no air beside it counts only when no surface block touches the point. For terrain, the splat weights of the
			// solid blocks carry the tiles: each is the trilinear weight of its cells, so across a triangle they change smoothly.
			const glm::vec3 b = p - glm::vec3(0.5f);
			const glm::vec3 base = glm::floor(b);
			const glm::vec3 t = b - base;
			int block = 0;
			float blockDistance = 1e30f;
			size_t dominant = 0;
			for (int c = 0; c < 8; ++c) {
				const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
				const int x = static_cast<int>(base.x) + dx, y = static_cast<int>(base.y) + dy, z = static_cast<int>(base.z) + dz;
				const float w = (dx ? t.x : 1.0f - t.x) * (dy ? t.y : 1.0f - t.y) * (dz ? t.z : 1.0f - t.z);
				if (!solidAt(chunk, group, x, y, z)) continue;
				const int id = chunk.at(x, y, z);
				// The solid block whose centre is nearest the point: a vertex on the ground takes the colour of the ground it stands on,
				// not of a neighbouring bump, whose centre can outweigh the ground's in the trilinear share.
				const glm::vec3 centre(x + 0.5f, y + 0.5f, z + 0.5f);
				const float distance = glm::dot(centre - p, centre - p) + (exposedBlock(chunk, x, y, z, group) ? 0.0f : 1000.0f);
				if (distance < blockDistance) {
					blockDistance = distance;
					dominant = chunk.index(x, y, z);
					block = id;
				}
			}
			if (block == 0) block = 1;

			// The splat weights. A terrain vertex takes them from its solid corners. A water vertex (the water's own mesh, whose
			// group is a transparent block) takes its water corners and the ground beside them, so its surface joins the ground. A
			// corner of another block (brick, wood) is a barrier the water does not blend across: only its water counts there.
			float splat[kSplatChannels] = {};
			float splatTotal = 0.0f;
			const bool blended = group != 0 && chunk.rule[group].blend && chunk.rule[group].splat >= 0;
			float waterWeight = 0.0f;
			bool barrier = false;
			for (int c = 0; c < 8; ++c) {
				const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
				const int x = static_cast<int>(base.x) + dx, y = static_cast<int>(base.y) + dy, z = static_cast<int>(base.z) + dz;
				const float w = (dx ? t.x : 1.0f - t.x) * (dy ? t.y : 1.0f - t.y) * (dz ? t.z : 1.0f - t.z);
				if (group == 0) {
					if (!solidAt(chunk, 0, x, y, z)) continue;
					const int channel = chunk.rule[chunk.at(x, y, z)].splat;
					if (channel >= 0) {
						splat[channel] += w;
						splatTotal += w;
					}
				} else if (blended) {
					const int id = blockAt(chunk, x, y, z);
					if (id == 0) continue;
					const PaddedChunk::MeshRule& rule = chunk.rule[id];
					if (rule.blend) {
						waterWeight += w;
					} else if (rule.splat >= 0) {
						splat[rule.splat] += w;
						splatTotal += w;
					} else if (rule.group != 0) {
						barrier = true;
					}
				}
			}
			if (blended) {
				if (barrier) {
					for (float& value : splat) value = 0.0f;
					splatTotal = 0.0f;
				}
				splat[chunk.rule[group].splat] += waterWeight;
				splatTotal += waterWeight;
			}

			// Ambient occlusion: the density a little way out along the normal. Walls close by darken the corner.
			float outside = densityAt(chunk, group,p + normal * 0.25f);
				if (stride > 1) {
					// A far vertex stands for a stride by stride patch of blocks, so its occlusion is the average over that patch. One
					// sample would darken the whole triangle between two far vertices (roadmap 15.8).
					const float reach = 0.5f * static_cast<float>(stride);
					float sum = 0.0f;
					for (int dz = -1; dz <= 1; ++dz) {
						for (int dx = -1; dx <= 1; ++dx) {
							sum += densityAt(chunk, group, p + normal * 0.25f + glm::vec3(dx * reach, 0.0f, dz * reach));
						}
					}
					outside = sum / 9.0f;
				}
			const float occlusion = std::clamp(1.0f - 0.6f * outside, 0.5f, 1.0f);

			VoxelVertex vertex;
			vertex.position = p;
			if (group == 0 || blended) {
				if (splatTotal <= 0.0f) {
					// No splat block among the corners: the density can come from the smoothed corners of blocks a step away, so a
					// vertex just off a ledge can have only air beside it. It takes the nearest splat block around it. A terrain vertex
					// must never fall back to the tile layers: the fragment shader's tiles are flat, so one such vertex would paint its
					// whole triangle with them (the green triangles on the sand).
					splatTotal = 1.0f;
					if (blended) splat[chunk.rule[group].splat] = 1.0f;
					else splat[nearestSplatChannel(chunk, p)] = 1.0f;
				}
				// The splat weights carry the tiles (the tile layers are not used), normalised to sum to 1.
				for (int k = 0; k < kSplatChannels; ++k) {
					const uint32_t byte = static_cast<uint32_t>(std::lround(std::clamp(splat[k] / splatTotal, 0.0f, 1.0f) * 255.0f));
					vertex.splat[k / 4] |= byte << (8 * (k % 4));
				}
				vertex.tiles = glm::vec3(255.0f);
			} else {
				vertex.tiles = tileLayers(chunk.rule[block]);
			}
			vertex.normal = normal;
			const glm::vec3 absolute = glm::abs(normal);
			if (absolute.y >= absolute.x && absolute.y >= absolute.z) {
				vertex.texCoord = glm::vec2(p.x, p.z);
			} else if (absolute.x >= absolute.z) {
				vertex.texCoord = glm::vec2(p.z, p.y);
			} else {
				vertex.texCoord = glm::vec2(p.x, p.y);
			}
			// Sky light of the air the normal points into: caves and the ground under trees are darker.
			// The light of the air the point looks into: the brightest air cell among those out along the normal and straight up. A
			// vertex whose normal points into a solid block takes the light of the air beside it, not the solid block's none.
			float sky = 0.0f;
			float lamp = 0.0f;
			// The eight cells that touch the point: the air among them is what the vertex can see.
			const glm::vec3 corner = glm::floor(p - glm::vec3(0.5f));
			bool sawAir = false;
			for (int c = 0; c < 8; ++c) {
				const int ax = static_cast<int>(corner.x) + (c & 1), ay = static_cast<int>(corner.y) + ((c >> 1) & 1), az = static_cast<int>(corner.z) + ((c >> 2) & 1);
				if (!lightPasses(chunk, chunk.at(ax, ay, az))) continue;
				sawAir = true;
				sky = std::max(sky, chunk.lightAt(ax, ay, az) / 255.0f);
				lamp = std::max(lamp, chunk.lampAt(ax, ay, az) / 255.0f);
			}
			// A vertex inside solid blocks (a surface pulled into a corner) has none of them: it takes the brightest air within a block.
			if (!sawAir) {
				for (int dz = -1; dz <= 2; ++dz) {
					for (int dy = -1; dy <= 2; ++dy) {
						for (int dx = -1; dx <= 2; ++dx) {
							const int ax = static_cast<int>(corner.x) + dx, ay = static_cast<int>(corner.y) + dy, az = static_cast<int>(corner.z) + dz;
							if (!lightPasses(chunk, chunk.at(ax, ay, az))) continue;
							sky = std::max(sky, chunk.lightAt(ax, ay, az) / 255.0f);
							lamp = std::max(lamp, chunk.lampAt(ax, ay, az) / 255.0f);
						}
					}
				}
			}
			// A far vertex takes the average light of the air over its patch, not the brightest cell at its corner (roadmap 15.8).
				if (stride > 1) {
					const float reach = 0.5f * static_cast<float>(stride);
					float skySum = 0.0f;
					int airCount = 0;
					for (int dz = -1; dz <= 1; ++dz) {
						for (int dx = -1; dx <= 1; ++dx) {
							const glm::vec3 q = p + glm::vec3(dx * reach, 0.0f, dz * reach);
							// The air row just above the point: the light the surface looks at.
							const int ax = static_cast<int>(std::floor(q.x - 0.5f)), ay = static_cast<int>(std::floor(q.y));
							const int az = static_cast<int>(std::floor(q.z - 0.5f));
							if (!lightPasses(chunk, chunk.at(ax, ay, az))) continue;
							skySum += chunk.lightAt(ax, ay, az) / 255.0f;
							++airCount;
						}
					}
					if (airCount > 0) sky = skySum / static_cast<float>(airCount);
				}
				vertex.colour = glm::vec4((chunk.rule[block].textured ? glm::vec3(1.0f) : palette[static_cast<size_t>(block)]) * occlusion
				* (chunk.damage.empty() ? 1.0f : 1.0f - DamageTint * chunk.damage[dominant] / 255.0f), 1.0f);
			vertex.skyDamage.x = sky;
			vertex.skyDamage.y = chunk.damage.empty() ? 0.0f : chunk.damage[dominant] / 255.0f;
			vertex.glow = glm::vec4(VoxelLightTint * (chunk.emission[block] / 255.0f * VoxelGlow)
				+ VoxelLightTint * (lamp * lampGain(sky)), 0.0f);
			return vertex;
		}
	}

	float densityAtPoint(const PaddedChunk& chunk, uint8_t group, glm::vec3 p) {
		return densityAt(chunk, group, p);
	}

	std::array<GroupBounds, 256> groupBounds(const PaddedChunk& chunk) {
		std::array<GroupBounds, 256> bounds{};
		const int b = chunk.border;
		for (int y = -b; y < chunk.height + b; ++y) {
			for (int z = -b; z < chunk.size + b; ++z) {
				for (int x = -b; x < chunk.size + b; ++x) {
					const int group = chunk.member[chunk.index(x, y, z)];
					if (group < 0) continue;
					GroupBounds& box = bounds[static_cast<size_t>(group)];
					const int at[3] = { x, y, z };
					for (int a = 0; a < 3; ++a) {
						box.lo[a] = std::min(box.lo[a], at[a]);
						box.hi[a] = std::max(box.hi[a], at[a]);
					}
				}
			}
		}
		return bounds;
	}

	namespace {
		// a / b rounded down and rounded up, for b > 0: the samples at or below and at or above a block's coordinate.
		int floorDivide(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
		int ceilDivide(int a, int b) { return -floorDivide(-a, b); }
	}

	void buildSmoothMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, int resolution, int stride,
		unsigned skirtSides, uint8_t group, const GroupBounds& bounds, MeshArrays& out) {
		out.vertices.clear();
		out.indices.clear();
		const int R = std::max(1, resolution);
		// A stride above 1 samples every stride-th block (a far chunk, meshed coarser). It needs one sample a block, and the
		// chunk's size must divide by it: the chunks are 16 blocks, so 2 is the largest stride the sample grid lines up on.
		const int T = R == 1 ? std::max(1, stride) : 1;
		const int S = chunk.size;
		const int H = chunk.height;
		// The sample grid's steps across the chunk: the size times R over T along x and z, and the height times R over T up.
		const int NX = S * R / T;
		const int NY = H * R / T;

		// Density samples, for i, k in [-1, S * R + 1] and j in [-1, H * R + 1]; array index = sample + 1. With one sample a
		// block, the samples sit at block centres, so a block one wide (a tree trunk) keeps its surface. With more, they sit at
		// i / R, which puts samples on block faces too and rounds the edges further.
		auto position = [R, T](int i, int j, int k) {
			if (R == 1) return glm::vec3(i * T, j * T, k * T) + glm::vec3(0.5f);
			return glm::vec3(i, j, k) / static_cast<float>(R);
		};
		const int sx = NX + 3;
		const int sy = NY + 3;
		// The grids are per thread and reused from chunk to chunk: a fresh allocation of each, zeroed, costs more than the samples.
		thread_local std::vector<float> samples;
		samples.assign(static_cast<size_t>(sx) * sy * sx, 0.0f);
		// A block group sampled at block centres (one sample a block) has the density 1 in its blocks and 0 elsewhere: densityAt's
		// weights are 1 at the block's own corner and 0 at the rest, so one lookup gives the same value as its eight.
		const bool occupancy = group != 0 && R == 1;
		// The samples that can be non-zero, per axis (0 is x, 1 is y, 2 is z), inclusive. An occupancy group is 0 at every sample
		// away from its blocks, and the grid starts at zero, so only the samples at its blocks, and the cells and edges that touch
		// them, need work. Everything else stays zero: the same values as a full pass.
		int lo[3] = { -1, -1, -1 };
		int hi[3] = { NX + 1, NY + 1, NX + 1 };
		if (occupancy) {
			for (int a = 0; a < 3; ++a) {
				lo[a] = std::max(lo[a], ceilDivide(bounds.lo[a], T));
				hi[a] = std::min(hi[a], floorDivide(bounds.hi[a], T));
			}
		} else if (group != 0 && bounds.lo[0] <= bounds.hi[0]) {
			// Half-block detail: a sample is the density of the two block centres either side of it, so it is 0 unless one of those
			// is in the group. The margins are one sample wide, which covers the rounding of the sample positions.
			for (int a = 0; a < 3; ++a) {
				lo[a] = std::max(lo[a], static_cast<int>(std::floor(R * (bounds.lo[a] - 0.5f))) - 1);
				hi[a] = std::min(hi[a], static_cast<int>(std::floor(R * (bounds.hi[a] + 1.5f))) + 1);
			}
		}
		auto sampleIndex = [sx](int i, int j, int k) { return static_cast<size_t>(((j + 1) * sx + (k + 1)) * sx + (i + 1)); };
		for (int j = lo[1]; j <= hi[1]; ++j) {
			for (int k = lo[2]; k <= hi[2]; ++k) {
				for (int i = lo[0]; i <= hi[0]; ++i) {
					samples[sampleIndex(i, j, k)] = occupancy ? (chunk.member[chunk.index(i * T, j * T, k * T)] == group ? 1.0f : 0.0f)
						: densityAt(chunk, group, position(i, j, k));
				}
			}
		}
		auto sample = [&](int i, int j, int k) { return samples[sampleIndex(i, j, k)]; };

		// Cells: the cube whose lowest corner is sample (i, j, k), for i and k in [-1, S * R] and j in [-1, H * R]. A cell
		// the surface crosses gets one vertex, at the average of its crossed edges.
		const int cn = NX + 2;
		const int cyn = NY + 2;
		auto cellIndex = [cn](int i, int j, int k) { return static_cast<size_t>(((j + 1) * cn + (k + 1)) * cn + (i + 1)); };
		thread_local std::vector<glm::vec3> cellPosition;
		cellPosition.assign(static_cast<size_t>(cn) * cyn * cn, glm::vec3(0.0f));
		for (int j = std::max(-1, lo[1] - 1); j <= std::min(NY, hi[1]); ++j) {
			for (int k = std::max(-1, lo[2] - 1); k <= std::min(NX, hi[2]); ++k) {
				for (int i = std::max(-1, lo[0] - 1); i <= std::min(NX, hi[0]); ++i) {
					float value[8];
					glm::vec3 corner[8];
					int inside = 0;
					for (int c = 0; c < 8; ++c) {
						const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
						value[c] = sample(i + dx, j + dy, k + dz);
						if (value[c] > SmoothIso) ++inside;
					}
					if (inside == 0 || inside == 8) continue;
					for (int c = 0; c < 8; ++c) corner[c] = position(i + (c & 1), j + ((c >> 1) & 1), k + ((c >> 2) & 1));
					glm::vec3 sum(0.0f);
					int crossings = 0;
					for (const auto& edge : CubeEdges) {
						const bool a = value[edge[0]] > SmoothIso;
						const bool b = value[edge[1]] > SmoothIso;
						if (a == b) continue;
						const float t = (SmoothIso - value[edge[0]]) / (value[edge[1]] - value[edge[0]]);
						sum += corner[edge[0]] + (corner[edge[1]] - corner[edge[0]]) * t;
						++crossings;
					}
					cellPosition[cellIndex(i, j, k)] = sum / static_cast<float>(crossings);
				}
			}
		}

		// Each cell's vertex is emitted once, when a quad first uses it.
		thread_local std::vector<int> emitted;
		emitted.assign(cellPosition.size(), -1);
		auto vertexOf = [&](int i, int j, int k) {
			const size_t index = cellIndex(i, j, k);
			if (emitted[index] < 0) {
				emitted[index] = static_cast<int>(out.vertices.size());
				out.vertices.push_back(smoothVertex(chunk, palette, group, cellPosition[index], stride));
			}
			return static_cast<unsigned int>(emitted[index]);
		};
		// A quad over the four cells around an edge, in counter-clockwise order seen from the edge's direction. An edge
		// that starts inside runs out of the solid, so its normal points along the edge; the others are turned round.
		auto emitQuad = [&](const int cells[4][3], bool startsInside) {
			unsigned int v[4];
			for (int n = 0; n < 4; ++n) v[n] = vertexOf(cells[n][0], cells[n][1], cells[n][2]);
			if (startsInside) {
				out.indices.insert(out.indices.end(), { v[0], v[1], v[2], v[0], v[2], v[3] });
			} else {
				out.indices.insert(out.indices.end(), { v[0], v[3], v[2], v[0], v[2], v[1] });
			}
		};

		// Each edge belongs to the chunk that owns its lowest sample, so chunks share no quads. Where a neighbour on the +x or +z side is
		// at another level of detail, the surface runs one cell past the chunk's last sample: the neighbour's surface stops short of
		// the shared plane, and the overlap covers the gap there (the skirts alone left a crack, roadmap 11.7).
		const int iEnd = (skirtSides & 2u) ? NX + 1 : NX;
		const int kEnd = (skirtSides & 8u) ? NX + 1 : NX;
		for (int j = std::max(0, lo[1] - 1); j <= std::min(NY - 1, hi[1]); ++j) {
			for (int k = std::max(0, lo[2] - 1); k <= std::min(kEnd - 1, hi[2]); ++k) {
				for (int i = std::max(0, lo[0] - 1); i <= std::min(iEnd - 1, hi[0]); ++i) {
					const bool here = sample(i, j, k) > SmoothIso;
					// x edge: the cells around it have lowest corners (i, j-1, k-1), (i, j, k-1), (i, j, k), (i, j-1, k).
					if (here != (sample(i + 1, j, k) > SmoothIso)) {
						const int cells[4][3] = { { i, j - 1, k - 1 }, { i, j, k - 1 }, { i, j, k }, { i, j - 1, k } };
						emitQuad(cells, here);
					}
					// y edge: (i-1, j, k-1), (i-1, j, k), (i, j, k), (i, j, k-1).
					if (here != (sample(i, j + 1, k) > SmoothIso)) {
						const int cells[4][3] = { { i - 1, j, k - 1 }, { i - 1, j, k }, { i, j, k }, { i, j, k - 1 } };
						emitQuad(cells, here);
					}
					// z edge: (i-1, j-1, k), (i, j-1, k), (i, j, k), (i-1, j, k).
					if (here != (sample(i, j, k + 1) > SmoothIso)) {
						const int cells[4][3] = { { i - 1, j - 1, k }, { i, j - 1, k }, { i, j, k }, { i - 1, j, k } };
						emitQuad(cells, here);
					}
				}
			}
		}

		// Skirts: where the surface crosses a chunk's side, a strip runs a block down from it. A neighbour at another level
		// of detail has its surface a little off the same line, and the strip covers the gap, so the seam is hidden.
		auto topOfColumn = [&](const auto& value, float& height) {
			for (int j = NY - 1; j >= -1; --j) {
				const float lower = value(j);
				const float upper = value(j + 1);
				if (lower > SmoothIso && upper <= SmoothIso) {
					// Only a surface with solid under it gets a skirt: over air, the strip would hang in the open.
					const int down = std::max(1, R / T);   // one block down: a step of the sample grid
					if (j - down < -1 || value(j - down) <= SmoothIso) return false;
					const float t = (lower - SmoothIso) / (lower - upper);
					const float y0 = position(0, j, 0).y;
					const float y1 = position(0, j + 1, 0).y;
					height = y0 + t * (y1 - y0);
					return true;
				}
			}
			return false;
		};
		auto addSkirt = [&](glm::vec3 a, glm::vec3 b, glm::vec3 normal) {
			const glm::vec3 down(0.0f, -1.0f, 0.0f);
			const unsigned int first = static_cast<unsigned int>(out.vertices.size());
			const glm::vec3 corners[4] = { a, b, b + down, a + down };
			for (const glm::vec3& corner : corners) {
				VoxelVertex vertex = smoothVertex(chunk, palette, group, corner, stride);
				vertex.normal = normal;
				out.vertices.push_back(vertex);
			}
			// Wound so the strip faces its normal.
			const glm::vec3 across = b - a;
			const glm::vec3 drop = down;
			if (glm::dot(glm::cross(across, drop), normal) >= 0.0f) {
				out.indices.insert(out.indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3 });
			} else {
				out.indices.insert(out.indices.end(), { first, first + 2, first + 1, first, first + 3, first + 2 });
			}
		};
		const int last = NX;
		// The two sides at x = first and last sample, along z.
		for (int side = 0; side < 2; ++side) {
			if (!(skirtSides & (side == 0 ? 1u : 2u))) continue;
			const int i = side == 0 ? 0 : last;
			const float x = position(i, 0, 0).x;
			for (int k = 0; k < last; ++k) {
				float heightA = 0.0f;
				float heightB = 0.0f;
				const bool foundA = topOfColumn([&](int j) { return sample(i, j, k); }, heightA);
				const bool foundB = topOfColumn([&](int j) { return sample(i, j, k + 1); }, heightB);
				if (!foundA || !foundB) continue;
				addSkirt(glm::vec3(x, heightA, position(0, 0, k).z), glm::vec3(x, heightB, position(0, 0, k + 1).z),
					glm::vec3(side == 0 ? -1.0f : 1.0f, 0.0f, 0.0f));
			}
		}
		// The two sides at z = first and last sample, along x.
		for (int side = 0; side < 2; ++side) {
			if (!(skirtSides & (side == 0 ? 4u : 8u))) continue;
			const int k = side == 0 ? 0 : last;
			const float z = position(0, 0, k).z;
			for (int i = 0; i < last; ++i) {
				float heightA = 0.0f;
				float heightB = 0.0f;
				const bool foundA = topOfColumn([&](int j) { return sample(i, j, k); }, heightA);
				const bool foundB = topOfColumn([&](int j) { return sample(i + 1, j, k); }, heightB);
				if (!foundA || !foundB) continue;
				addSkirt(glm::vec3(position(i, 0, 0).x, heightA, z), glm::vec3(position(i + 1, 0, 0).x, heightB, z),
					glm::vec3(0.0f, 0.0f, side == 0 ? -1.0f : 1.0f));
			}
		}
	}
	namespace {
		// A box of six faces, with one colour, glow and sky light, appended to out.
		void addBox(MeshArrays& out, glm::vec3 centre, glm::vec3 half, glm::vec3 colour, glm::vec3 glow, float sky, float flicker = 0.0f) {
			for (int axis = 0; axis < 3; ++axis) {
				const int u = (axis + 1) % 3;
				const int v = (axis + 2) % 3;
				for (int sign = -1; sign <= 1; sign += 2) {
					glm::vec3 normal(0.0f);
					normal[axis] = static_cast<float>(sign);
					const unsigned int first = static_cast<unsigned int>(out.vertices.size());
					for (int k = 0; k < 4; ++k) {
						glm::vec3 position = centre;
						position[axis] += normal[axis] * half[axis];
						position[u] += (cornerU[k] ? 1.0f : -1.0f) * half[u];
						position[v] += (cornerV[k] ? 1.0f : -1.0f) * half[v];
						VoxelVertex vertex;
						vertex.position = position;
						vertex.normal = normal;
						vertex.texCoord = glm::vec2(position[u], position[v]);
						vertex.colour = glm::vec4(colour, 1.0f);
						vertex.glow = glm::vec4(glow, flicker);
						vertex.skyDamage.x = sky;
						out.vertices.push_back(vertex);
					}
					out.indices.insert(out.indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3 });
				}
			}
		}
	}

	// The half size of a box with the given axes and half sizes, along each world axis (for the axis-aligned bound of a stick).
	glm::vec3 boundOf(const glm::vec3 axes[3], glm::vec3 half) {
		glm::vec3 bound(0.0f);
		for (int j = 0; j < 3; ++j) {
			for (int i = 0; i < 3; ++i) bound[i] += std::abs(axes[j][i]) * half[j];
		}
		return bound;
	}

	PropShape propShape(glm::vec3 cell, int dir, float anchor, glm::vec3 normal) {
		const glm::vec3 wall(PropDirs[dir][0], PropDirs[dir][1], PropDirs[dir][2]);
		const bool ceiling = wall.y > 0.5f;
		const bool floor = wall.y < -0.5f;
		// The foot is on the skin of the block the torch leans on, at anchor toward it. A side torch sits low in its cell.
		glm::vec3 base = cell + wall * anchor;
		if (!floor && !ceiling) base.y -= 0.2f;
		// The stick stands out of the skin along the surface normal, tilted up a little so a torch on a wall or a slope still stands up.
		const glm::vec3 n = glm::length(normal) > 1e-4f ? glm::normalize(normal) : -wall;
		const glm::vec3 upward = ceiling ? glm::vec3(0.0f, -1.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::vec3 along = glm::normalize(n + upward * 0.5f);
		glm::vec3 side = glm::cross(along, glm::vec3(0.0f, 0.0f, 1.0f));
		if (glm::length(side) < 1e-3f) side = glm::cross(along, glm::vec3(1.0f, 0.0f, 0.0f));
		side = glm::normalize(side);
		const glm::vec3 third = glm::cross(along, side);
		// A side torch's stick is kept just off the wall, as it was when the sticks were upright.
		const glm::vec3 foot = base + (floor || ceiling ? glm::vec3(0.0f) : n * 0.06f);
		PropShape shape;
		shape.stickAxes[0] = side;
		shape.stickAxes[1] = along;
		shape.stickAxes[2] = third;
		shape.stickSize = glm::vec3(0.06f, 0.25f, 0.06f);
		shape.stickCentre = foot + along * 0.25f;
		shape.stickHalf = boundOf(shape.stickAxes, shape.stickSize);
		shape.flameCentre = foot + along * 0.55f;
		shape.flameHalf = glm::vec3(0.1f, 0.08f, 0.1f);
		return shape;
	}

	// A box with its own axes (orthonormal): half is its half size along each axis, and the faces get the axes' normals.
	void addOrientedBox(MeshArrays& out, glm::vec3 centre, const glm::vec3 axes[3], glm::vec3 half, glm::vec3 colour, glm::vec3 glow,
		float sky, float flicker = 0.0f) {
		for (int axis = 0; axis < 3; ++axis) {
			const int u = (axis + 1) % 3;
			const int v = (axis + 2) % 3;
			for (int sign = -1; sign <= 1; sign += 2) {
				const glm::vec3 normal = axes[axis] * static_cast<float>(sign);
				const unsigned int first = static_cast<unsigned int>(out.vertices.size());
				for (int k = 0; k < 4; ++k) {
					const glm::vec3 position = centre + normal * half[axis] + axes[u] * ((cornerU[k] ? 1.0f : -1.0f) * half[u])
						+ axes[v] * ((cornerV[k] ? 1.0f : -1.0f) * half[v]);
					VoxelVertex vertex;
					vertex.position = position;
					vertex.normal = normal;
					vertex.texCoord = glm::vec2(glm::dot(position, axes[u]), glm::dot(position, axes[v]));
					vertex.colour = glm::vec4(colour, 1.0f);
					vertex.glow = glm::vec4(glow, flicker);
					vertex.skyDamage.x = sky;
					out.vertices.push_back(vertex);
				}
				out.indices.insert(out.indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3 });
			}
		}
	}

	void buildPropMesh(const PaddedChunk& chunk, MeshArrays& out) {
		const glm::vec3 wood(0.25f, 0.15f, 0.07f);
		// The flame's own colour is dark, so its glow (the light colour) is what shows: it stays warm, not white.
		const glm::vec3 flame(0.25f, 0.15f, 0.06f);
		for (const PaddedChunk::Prop& prop : chunk.props) {
			if (prop.x < 0 || prop.x >= chunk.size || prop.y < 0 || prop.y >= chunk.height || prop.z < 0 || prop.z >= chunk.size) continue;
			const float sky = chunk.lightAt(prop.x, prop.y, prop.z) / 255.0f;
			const glm::vec3 cell(prop.x + 0.5f, prop.y + 0.5f, prop.z + 0.5f);
			const PropShape shape = propShape(cell, prop.dir, prop.anchor, prop.normal);
			addOrientedBox(out, shape.stickCentre, shape.stickAxes, shape.stickSize, wood, glm::vec3(0.0f), sky);
			// Each torch's flame wavers on its own phase, from its cell (a fraction in 0 to 1, then 1 added so it is never 0).
			const float seed = std::sin(prop.x * 12.9898f + prop.y * 37.719f + prop.z * 78.233f) * 43758.5453f;
			const float phase = 1.0f + (seed - std::floor(seed));
			addBox(out, shape.flameCentre, shape.flameHalf, flame, VoxelLightTint * VoxelGlow, sky, phase);
		}
	}

	void buildBlob(const std::array<glm::vec3, 256>& palette, uint8_t block, glm::vec3 centre, float radius, MeshArrays& out) {
		// A round ball of the block's colour: the look of a block in flight, which is smooth rather than cubic.
		const int rings = 8;
		const int segments = 12;
		const float pi = 3.14159265f;
		const unsigned int first = static_cast<unsigned int>(out.vertices.size());
		for (int r = 0; r <= rings; ++r) {
			const float phi = pi * static_cast<float>(r) / static_cast<float>(rings);
			for (int s = 0; s <= segments; ++s) {
				const float theta = 2.0f * pi * static_cast<float>(s) / static_cast<float>(segments);
				const glm::vec3 normal(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
				VoxelVertex vertex;
				vertex.position = centre + normal * radius;
				vertex.normal = normal;
				vertex.texCoord = glm::vec2(theta / (2.0f * pi), phi / pi);
				vertex.colour = glm::vec4(palette[static_cast<size_t>(block)], 1.0f);
				out.vertices.push_back(vertex);
			}
		}
		for (int r = 0; r < rings; ++r) {
			for (int s = 0; s < segments; ++s) {
				const unsigned int a = first + static_cast<unsigned int>(r * (segments + 1) + s);
				const unsigned int b = a + static_cast<unsigned int>(segments + 1);
				out.indices.insert(out.indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
			}
		}
	}
}
