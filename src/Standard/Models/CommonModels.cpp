#include <cmath>
#include <utility>
#include "../../Core/Engine/Engine.h"
#include "CommonModels.h"

namespace CGEngine {
    MeshData* getCubeModel(float scale) {
        auto cubeAsset = assets.create<MeshData>("default_cube", getCubeVertices(scale), getCubeIndices());
        if (!cubeAsset.has_value()) return nullptr;
        optional<id_t> cubeId = cubeAsset.value().first;
		return assets.get<MeshData>(cubeId.value());
    }

    MeshData* getPlaneModel(float scale, float textureId, Vector3f offset) {
        auto planeAsset = assets.create<MeshData>("default_plane", getPlaneVertices(scale, textureId, offset), getPlaneIndices());
		if (!planeAsset.has_value()) return nullptr;
        optional<id_t> planeId = planeAsset.value().first;
		return assets.get<MeshData>(planeId.value());
    }

    vector<VertexData> getSphereVertices(float radius, int segments, int rings) {
        vector<VertexData> vertices;
        const float pi = 3.14159265358979f;
        for (int r = 0; r <= rings; ++r) {
            const float theta = pi * static_cast<float>(r) / static_cast<float>(rings);
            for (int s = 0; s <= segments; ++s) {
                const float phi = 2.0f * pi * static_cast<float>(s) / static_cast<float>(segments);
                const glm::vec3 unit(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
                const float u = static_cast<float>(s) / static_cast<float>(segments);
                const float v = static_cast<float>(r) / static_cast<float>(rings);
                vertices.push_back(VertexData({ unit.x * radius, unit.y * radius, unit.z * radius }, { u, v }, { unit.x, unit.y, unit.z }, 0));
            }
        }
        return vertices;
    }

    vector<unsigned int> getSphereIndices(int segments, int rings, const vector<VertexData>& vertices) {
        // Two triangles per cell of the grid. Each is wound counter-clockwise seen from outside: a triangle whose winding
        // points inwards (against the vertex normals) is turned round, so backface culling keeps the outside.
        vector<unsigned int> indices;
        const unsigned int row = static_cast<unsigned int>(segments + 1);
        auto addTriangle = [&](unsigned int a, unsigned int b, unsigned int c) {
            if (!vertices.empty()) {
                const auto& pa = vertices[a].position;
                const auto& pb = vertices[b].position;
                const auto& pc = vertices[c].position;
                const auto& n = vertices[a].normal;
                const float ux = pb[0] - pa[0], uy = pb[1] - pa[1], uz = pb[2] - pa[2];
                const float wx = pc[0] - pa[0], wy = pc[1] - pa[1], wz = pc[2] - pa[2];
                const float facing = (uy * wz - uz * wy) * n[0] + (uz * wx - ux * wz) * n[1] + (ux * wy - uy * wx) * n[2];
                if (facing < 0.0f) std::swap(b, c);
            }
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
        };
        for (int r = 0; r < rings; ++r) {
            for (int s = 0; s < segments; ++s) {
                const unsigned int a = static_cast<unsigned int>(r) * row + static_cast<unsigned int>(s);
                const unsigned int b = a + row;
                addTriangle(a, b, a + 1);
                addTriangle(a + 1, b, b + 1);
            }
        }
        return indices;
    }

    vector<VertexData> getCubeVertices(float scale) {
        return {
            // positions            // uv           // normals
            //bottom
            VertexData({-scale,-scale,-scale}, {0.0f, 0.0f}, {0.0f,-1.0f,0.0f}, 0),
            VertexData({ scale,-scale,-scale}, {1.0f, 0.0f}, {0.0f,-1.0f,0.0f}, 0),
            VertexData({-scale,-scale, scale}, {0.0f, 1.0f}, {0.0f,-1.0f,0.0f}, 0),
            VertexData({ scale,-scale,-scale}, {1.0f, 0.0f}, {0.0f,-1.0f,0.0f}, 0),
            VertexData({ scale,-scale, scale}, {1.0f, 1.0f}, {0.0f,-1.0f,0.0f}, 0),
            VertexData({-scale,-scale, scale}, {0.0f, 1.0f}, {0.0f,-1.0f,0.0f}, 0),
            //TODO: CONTINUE REPLACING VERTEXDATA
            // top
            VertexData({-scale, scale,-scale}, {0.0f, 0.0f}, {0.0f,1.0f,0.0f}, 0),
            VertexData({-scale, scale, scale}, {0.0f, 1.0f}, {0.0f,1.0f,0.0f}, 0),
            VertexData({ scale, scale,-scale}, {1.0f, 0.0f}, {0.0f,1.0f,0.0f}, 0),
            VertexData({ scale, scale,-scale}, {1.0f, 0.0f}, {0.0f,1.0f,0.0f}, 0),
            VertexData({-scale, scale, scale}, {0.0f, 1.0f}, {0.0f,1.0f,0.0f}, 0),
            VertexData({ scale, scale, scale}, {1.0f, 1.0f}, {0.0f,1.0f,0.0f}, 0),

             // front
            VertexData({-scale,-scale, scale}, {1.0f, 0.0f}, {0.0f,0.0f,1.0f}, 0),
            VertexData({ scale,-scale, scale}, {0.0f, 0.0f}, {0.0f,0.0f,1.0f}, 0),
            VertexData({-scale, scale, scale}, {1.0f, 1.0f}, {0.0f,0.0f,1.0f}, 0),
            VertexData({ scale,-scale, scale}, {0.0f, 0.0f}, {0.0f,0.0f,1.0f}, 0),
            VertexData({ scale, scale, scale}, {0.0f, 1.0f}, {0.0f,0.0f,1.0f}, 0),
            VertexData({-scale, scale, scale}, {1.0f, 1.0f}, {0.0f,0.0f,1.0f}, 0),

             // back
            VertexData({-scale,-scale,-scale}, {0.0f, 0.0f}, {0.0f,0.0f,-1.0f}, 0),
            VertexData({-scale, scale,-scale}, {0.0f, 1.0f}, {0.0f,0.0f,-1.0f}, 0),
            VertexData({ scale,-scale,-scale}, {1.0f, 0.0f}, {0.0f,0.0f,-1.0f}, 0),
            VertexData({ scale,-scale,-scale}, {1.0f, 0.0f}, {0.0f,0.0f,-1.0f}, 0),
            VertexData({-scale, scale,-scale}, {0.0f, 1.0f}, {0.0f,0.0f,-1.0f}, 0),
            VertexData({ scale, scale,-scale}, {1.0f, 1.0f}, {0.0f,0.0f,-1.0f}, 0),

              // left
            VertexData({-scale,-scale, scale}, {0.0f, 1.0f}, {-1.0f,0.0f,0.0f}, 0),
            VertexData({-scale, scale,-scale}, {1.0f, 0.0f}, {-1.0f,0.0f,0.0f}, 0),
            VertexData({-scale,-scale,-scale}, {0.0f, 0.0f}, {-1.0f,0.0f,0.0f}, 0),
            VertexData({-scale,-scale, scale}, {0.0f, 1.0f}, {-1.0f,0.0f,0.0f}, 0),
            VertexData({-scale, scale, scale}, {1.0f, 1.0f}, {-1.0f,0.0f,0.0f}, 0),
            VertexData({-scale, scale,-scale}, {1.0f, 0.0f}, {-1.0f,0.0f,0.0f}, 0),

              // right
            VertexData({ scale,-scale, scale}, {1.0f, 1.0f}, {1.0f,0.0f,0.0f}, 0),
            VertexData({ scale,-scale,-scale}, {1.0f, 0.0f}, {1.0f,0.0f,0.0f}, 0),
            VertexData({ scale, scale,-scale}, {0.0f, 0.0f}, {1.0f,0.0f,0.0f}, 0),
            VertexData({ scale,-scale, scale}, {1.0f, 1.0f}, {1.0f,0.0f,0.0f}, 0),
            VertexData({ scale, scale,-scale}, {0.0f, 0.0f}, {1.0f,0.0f,0.0f}, 0),
            VertexData({ scale, scale, scale}, {0.0f, 1.0f}, {1.0f,0.0f,0.0f}, 0),
        };
    }

    vector<unsigned int> getCubeIndices() {
        return {
           0,1,2,
           3,4,5,
           6,7,8,
           9,10,11,
           12,13,14,
           15,16,17,
           18,19,20,
           21,22,23,
           24,25,26,
           27,28,29,
           30,31,32,
           33,34,35
        };
    }

    vector<VertexData> getPlaneVertices(float scale, float textureId) {
        return {
            VertexData({-scale,-scale, scale}, {1.0f, 0.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({ scale,-scale, scale}, {0.0f, 0.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({-scale, scale, scale}, {1.0f, 1.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({ scale,-scale, scale}, {0.0f, 0.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({ scale, scale, scale}, {0.0f, 1.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({-scale, scale, scale}, {1.0f, 1.0f},{0.0f,0.0f,1.0f},textureId),
        };
    }

    vector<unsigned int> getPlaneIndices() {
        return {
           0,1,2,
           3,4,5
        };
    }

    vector<VertexData> getPlaneVertices(float scale, float textureId, Vector3f offset) {
        return {
            VertexData({-scale+offset.x,-scale+offset.y, scale+offset.z}, {1.0f, 0.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({ scale+offset.x,-scale+offset.y, scale+offset.z}, {0.0f, 0.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({-scale+offset.x, scale+offset.y, scale+offset.z}, {1.0f, 1.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({ scale+offset.x,-scale+offset.y, scale+offset.z}, {0.0f, 0.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({ scale+offset.x, scale+offset.y, scale+offset.z}, {0.0f, 1.0f},{0.0f,0.0f,1.0f},textureId),
            VertexData({-scale+offset.x, scale+offset.y, scale+offset.z}, {1.0f, 1.0f},{0.0f,0.0f,1.0f},textureId),
        };
    }

    vector<unsigned int> getTilemapIndices(Vector2i size) {
        vector<unsigned int> map = {};
        unsigned int firstId = 0;
        for (int col = 0; col < size.y; ++col) {
            for (int row = 0; row < size.x; ++row) {
                firstId = (col * size.x * 6) + (row * 6);
                vector<unsigned int> indices =  {
                   firstId + 0,firstId + 1,firstId + 2,
                   firstId + 3,firstId + 4,firstId + 5
                };
                map.insert(map.end(), indices.begin(), indices.end());
            }
        }
        return map;
    }

    MeshData* getTilemapModel(float scale, Vector2i size, vector<vector<int>> textureMap) {
        auto tilemapAsset = assets.create<MeshData>("default_tilemap", getTilemapVertices(scale, size, textureMap), getTilemapIndices(size));
		if (!tilemapAsset.has_value()) return nullptr;
		optional<id_t> tilemapId = tilemapAsset.value().first;
		return assets.get<MeshData>(tilemapId.value());
    }

    vector<VertexData> getTilemapVertices(float scale, Vector2i size, vector<vector<int>> textureMap){
        vector<VertexData> map = {};
        for (int col = 0; col < size.y; ++col) {
            for (int row = 0; row < size.x; ++row) {
                auto textureId = 0;
                if (col < textureMap.size() && row < textureMap.at(col).size()) {
                    textureId = textureMap.at(col).at(row);
                }
                vector<VertexData> plane = getPlaneVertices(scale, textureId, Vector3f{ row * (scale * 2),-col * (scale * 2),0 });
                map.insert(map.end(), plane.begin(), plane.end());
            }
        }
        return map;
    }
}