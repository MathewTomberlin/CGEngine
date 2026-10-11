#include <algorithm>
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

	namespace {
		// The surface is where the density crosses this: a sample inside the solid blocks is above it.
		const float SmoothIso = 0.5f;

		// The 12 edges of a cell's cube, as pairs of corner indices (corner bit 0 is x, bit 1 is y, bit 2 is z).
		const int CubeEdges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };

		// Whether the block at (x, y, z) is solid. Outside the padded chunk it is air.
		bool solidAt(const PaddedChunk& chunk, int x, int y, int z) {
			const int b = chunk.border;
			if (x < -b || y < -b || z < -b || x >= chunk.size + b || y >= chunk.height + b || z >= chunk.size + b) return false;
			return chunk.at(x, y, z) != 0;
		}

		// The density at p (block units): the blocks' occupancy, interpolated between block centres (a block's centre is
		// its corner + 0.5). It falls from 1 to 0 across each block face, so the iso-surface at 0.5 runs along the faces
		// and rounds the convex edges and corners.
		float densityAt(const PaddedChunk& chunk, glm::vec3 p) {
			const glm::vec3 b = p - glm::vec3(0.5f);
			const glm::vec3 base = glm::floor(b);
			const glm::vec3 t = b - base;
			const int x0 = static_cast<int>(base.x), y0 = static_cast<int>(base.y), z0 = static_cast<int>(base.z);
			float value = 0.0f;
			for (int c = 0; c < 8; ++c) {
				const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
				const float w = (dx ? t.x : 1.0f - t.x) * (dy ? t.y : 1.0f - t.y) * (dz ? t.z : 1.0f - t.z);
				if (solidAt(chunk, x0 + dx, y0 + dy, z0 + dz)) value += w;
			}
			return value;
		}

		// The vertex at p (chunk block units): a normal from the density gradient, the colour of the solid block that
		// weighs most there, darkened where the air beside it is walled in, and a texture coordinate from the normal's axis.
		VoxelVertex smoothVertex(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, glm::vec3 p) {
			const float h = 0.25f;
			const glm::vec3 gradient(
				densityAt(chunk, p + glm::vec3(h, 0, 0)) - densityAt(chunk, p - glm::vec3(h, 0, 0)),
				densityAt(chunk, p + glm::vec3(0, h, 0)) - densityAt(chunk, p - glm::vec3(0, h, 0)),
				densityAt(chunk, p + glm::vec3(0, 0, h)) - densityAt(chunk, p - glm::vec3(0, 0, h)));
			const float length = glm::length(gradient);
			const glm::vec3 normal = length > 1e-6f ? -gradient / length : glm::vec3(0.0f, 1.0f, 0.0f);

			// The dominant solid block: the one whose centre weighs most at p.
			const glm::vec3 b = p - glm::vec3(0.5f);
			const glm::vec3 base = glm::floor(b);
			const glm::vec3 t = b - base;
			int block = 0;
			float blockWeight = -1.0f;
			for (int c = 0; c < 8; ++c) {
				const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
				const int x = static_cast<int>(base.x) + dx, y = static_cast<int>(base.y) + dy, z = static_cast<int>(base.z) + dz;
				const float w = (dx ? t.x : 1.0f - t.x) * (dy ? t.y : 1.0f - t.y) * (dz ? t.z : 1.0f - t.z);
				if (solidAt(chunk, x, y, z) && w > blockWeight) {
					blockWeight = w;
					block = chunk.at(x, y, z);
				}
			}
			if (block == 0) block = 1;

			// Ambient occlusion: the density a little way out along the normal. Walls close by darken the corner.
			const float outside = densityAt(chunk, p + normal * 0.25f);
			const float occlusion = std::clamp(1.0f - 0.6f * outside, 0.5f, 1.0f);

			VoxelVertex vertex;
			vertex.position = p;
			vertex.normal = normal;
			const glm::vec3 absolute = glm::abs(normal);
			if (absolute.y >= absolute.x && absolute.y >= absolute.z) {
				vertex.texCoord = glm::vec2(p.x, p.z);
			} else if (absolute.x >= absolute.z) {
				vertex.texCoord = glm::vec2(p.z, p.y);
			} else {
				vertex.texCoord = glm::vec2(p.x, p.y);
			}
			vertex.colour = palette[static_cast<size_t>(block)] * occlusion;
			return vertex;
		}
	}

	void buildSmoothMesh(const PaddedChunk& chunk, const std::array<glm::vec3, 256>& palette, int resolution, MeshArrays& out) {
		out.vertices.clear();
		out.indices.clear();
		const int R = std::max(1, resolution);
		const int S = chunk.size;
		const int H = chunk.height;

		// Density samples at i / R (x and z) and j / R (y), for i, k in [-1, S * R + 1] and j in [-1, H * R + 1].
		// Array index = sample + 1.
		const int sx = S * R + 3;
		const int sy = H * R + 3;
		std::vector<float> samples(static_cast<size_t>(sx) * sy * sx);
		auto sampleIndex = [sx](int i, int j, int k) { return static_cast<size_t>(((j + 1) * sx + (k + 1)) * sx + (i + 1)); };
		for (int j = -1; j <= H * R + 1; ++j) {
			for (int k = -1; k <= S * R + 1; ++k) {
				for (int i = -1; i <= S * R + 1; ++i) {
					samples[sampleIndex(i, j, k)] = densityAt(chunk, glm::vec3(i, j, k) / static_cast<float>(R));
				}
			}
		}
		auto sample = [&](int i, int j, int k) { return samples[sampleIndex(i, j, k)]; };

		// Cells: the cube whose lowest corner is sample (i, j, k), for i and k in [-1, S * R] and j in [-1, H * R]. A cell
		// the surface crosses gets one vertex, at the average of its crossed edges.
		const int cn = S * R + 2;
		const int cyn = H * R + 2;
		auto cellIndex = [cn](int i, int j, int k) { return static_cast<size_t>(((j + 1) * cn + (k + 1)) * cn + (i + 1)); };
		std::vector<glm::vec3> cellPosition(static_cast<size_t>(cn) * cyn * cn);
		for (int j = -1; j <= H * R; ++j) {
			for (int k = -1; k <= S * R; ++k) {
				for (int i = -1; i <= S * R; ++i) {
					float value[8];
					glm::vec3 corner[8];
					int inside = 0;
					for (int c = 0; c < 8; ++c) {
						const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
						value[c] = sample(i + dx, j + dy, k + dz);
						corner[c] = glm::vec3(i + dx, j + dy, k + dz) / static_cast<float>(R);
						if (value[c] > SmoothIso) ++inside;
					}
					if (inside == 0 || inside == 8) continue;
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
		std::vector<int> emitted(cellPosition.size(), -1);
		auto vertexOf = [&](int i, int j, int k) {
			const size_t index = cellIndex(i, j, k);
			if (emitted[index] < 0) {
				emitted[index] = static_cast<int>(out.vertices.size());
				out.vertices.push_back(smoothVertex(chunk, palette, cellPosition[index]));
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

		// Each edge belongs to the chunk that owns its lowest sample, so chunks share no quads.
		for (int j = 0; j < H * R; ++j) {
			for (int k = 0; k < S * R; ++k) {
				for (int i = 0; i < S * R; ++i) {
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
	}
}
