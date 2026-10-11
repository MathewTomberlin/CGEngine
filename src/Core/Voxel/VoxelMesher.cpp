#include "VoxelMesher.h"

namespace CGEngine {
	namespace {
		// A cell of the face mask for one plane: the block whose face is there (0 for none), which way it points,
		// and the occlusion of its four corners. Cells merge only when all of these match.
		struct MaskCell {
			uint8_t block = 0;
			int8_t sign = 0;
			uint8_t occlusion[4] = { 0, 0, 0, 0 };

			bool operator==(const MaskCell& other) const {
				return block == other.block && sign == other.sign
					&& occlusion[0] == other.occlusion[0] && occlusion[1] == other.occlusion[1]
					&& occlusion[2] == other.occlusion[2] && occlusion[3] == other.occlusion[3];
			}
		};

		// Brightness of a corner by its occlusion (0 = walled in, 3 = open).
		const float occlusionLight[4] = { 0.5f, 0.66f, 0.82f, 1.0f };

		// Brightness of a face by its direction: top full strength, sides darker, bottom darkest.
		float faceShade(int axis, int sign) {
			if (axis == 1) return sign > 0 ? 1.0f : 0.5f;
			if (axis == 0) return 0.8f;
			return 0.65f;
		}

		// The corners of a face in its (u, v) axes, in order around the face.
		const int cornerU[4] = { 0, 1, 1, 0 };
		const int cornerV[4] = { 0, 0, 1, 1 };

		// Occlusion of each corner of a face, from the air cell in front of it. Each corner has two cells beside it and
		// one diagonal, all in the air layer; two solid sides close the corner completely.
		void cornerOcclusion(const PaddedChunk& chunk, const int air[3], int u, int v, uint8_t out[4]) {
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
				const bool s1 = chunk.at(side1[0], side1[1], side1[2]) != 0;
				const bool s2 = chunk.at(side2[0], side2[1], side2[2]) != 0;
				const bool sc = chunk.at(corner[0], corner[1], corner[2]) != 0;
				out[k] = (s1 && s2) ? 0 : static_cast<uint8_t>(3 - (s1 + s2 + sc));
			}
		}

		// One quad covering w by h cells of the mask, on plane s of axis d, with (u, v) its other two axes.
		void emitQuad(MeshArrays& out, const std::array<glm::vec3, 256>& palette, int d, int u, int v, int sign, int s,
			int i, int j, int w, int h, const MaskCell& face) {
			const unsigned int first = static_cast<unsigned int>(out.vertices.size());
			glm::vec3 normal(0.0f);
			normal[d] = static_cast<float>(sign);
			const glm::vec3 base = palette[face.block] * faceShade(d, sign);
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
				vertex.colour = base * occlusionLight[face.occlusion[k]];
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

	void buildChunkMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, MeshArrays& out) {
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
							const uint8_t block = chunk.at(solid[0], solid[1], solid[2]);
							if (block == 0 || chunk.at(air[0], air[1], air[2]) != 0) continue;
							cell.block = block;
							cell.sign = static_cast<int8_t>(sign);
							cornerOcclusion(chunk, air, u, v, cell.occlusion);
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
}
