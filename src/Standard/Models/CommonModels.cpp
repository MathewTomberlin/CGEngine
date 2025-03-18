#include "CommonModels.h"

namespace CGEngine {
    MeshData getCubeModel(float scale) {
        return MeshData(getCubeVertices(scale),getCubeIndices());
    }

    MeshData getPlaneModel(float scale, float textureId, Vector3f offset) {
        return MeshData(getPlaneVertices(scale, textureId, offset), getPlaneIndices());
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

    MeshData getTilemapModel(float scale, Vector2i size, vector<vector<int>> textureMap) {
        return MeshData(getTilemapVertices(scale, size, textureMap), getTilemapIndices(size));
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