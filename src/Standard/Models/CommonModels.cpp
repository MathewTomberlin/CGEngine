#include "CommonModels.h"

namespace CGEngine {
    MeshData getCubeModel(float scale) {
        return MeshData(getCubeVertices(scale));
    }

    MeshData getPlaneModel(float scale, float textureId, Vector3f offset) {
        return MeshData(getPlaneVertices(scale, textureId, offset));
    }

    vector<float> getCubeVertices(float scale) {
        return {
            // positions            // uv           // normals
            //bottom
            -scale,-scale,-scale,   0.0f, 0.0f,     0.0f,-1.0f,0.0f,0,
             scale,-scale,-scale,   1.0f, 0.0f,     0.0f,-1.0f,0.0f,0,
            -scale,-scale, scale,   0.0f, 1.0f,     0.0f,-1.0f,0.0f,0,
             scale,-scale,-scale,   1.0f, 0.0f,     0.0f,-1.0f,0.0f,0,
             scale,-scale, scale,   1.0f, 1.0f,     0.0f,-1.0f,0.0f,0,
            -scale,-scale, scale,   0.0f, 1.0f,     0.0f,-1.0f,0.0f,0,

            // top
            -scale, scale,-scale,   0.0f, 0.0f,     0.0f,1.0f,0.0f,0,
            -scale, scale, scale,   0.0f, 1.0f,     0.0f,1.0f,0.0f,0,
             scale, scale,-scale,   1.0f, 0.0f,     0.0f,1.0f,0.0f,0,
             scale, scale,-scale,   1.0f, 0.0f,     0.0f,1.0f,0.0f,0,
            -scale, scale, scale,   0.0f, 1.0f,     0.0f,1.0f,0.0f,0,
             scale, scale, scale,   1.0f, 1.0f,     0.0f,1.0f,0.0f,0,

             // front
             -scale,-scale, scale,   1.0f, 0.0f,    0.0f,0.0f,1.0f,0,
              scale,-scale, scale,   0.0f, 0.0f,    0.0f,0.0f,1.0f,0,
             -scale, scale, scale,   1.0f, 1.0f,    0.0f,0.0f,1.0f,0,
              scale,-scale, scale,   0.0f, 0.0f,    0.0f,0.0f,1.0f,0,
              scale, scale, scale,   0.0f, 1.0f,    0.0f,0.0f,1.0f,0,
             -scale, scale, scale,   1.0f, 1.0f,    0.0f,0.0f,1.0f,0,

             // back
             -scale,-scale,-scale,   0.0f, 0.0f,    0.0f,0.0f,-1.0f,0,
             -scale, scale,-scale,   0.0f, 1.0f,    0.0f,0.0f,-1.0f,0,
              scale,-scale,-scale,   1.0f, 0.0f,    0.0f,0.0f,-1.0f,0,
              scale,-scale,-scale,   1.0f, 0.0f,    0.0f,0.0f,-1.0f,0,
             -scale, scale,-scale,   0.0f, 1.0f,    0.0f,0.0f,-1.0f,0,
              scale, scale,-scale,   1.0f, 1.0f,    0.0f,0.0f,-1.0f,0,

              // left
              -scale,-scale, scale,   0.0f, 1.0f,   -1.0f,0.0f,0.0f,0,
              -scale, scale,-scale,   1.0f, 0.0f,   -1.0f,0.0f,0.0f,0,
              -scale,-scale,-scale,   0.0f, 0.0f,   -1.0f,0.0f,0.0f,0,
              -scale,-scale, scale,   0.0f, 1.0f,   -1.0f,0.0f,0.0f,0,
              -scale, scale, scale,   1.0f, 1.0f,   -1.0f,0.0f,0.0f,0,
              -scale, scale,-scale,   1.0f, 0.0f,   -1.0f,0.0f,0.0f,0,

              // right
               scale,-scale, scale,   1.0f, 1.0f,   1.0f,0.0f,0.0f,0,
               scale,-scale,-scale,   1.0f, 0.0f,   1.0f,0.0f,0.0f,0,
               scale, scale,-scale,   0.0f, 0.0f,   1.0f,0.0f,0.0f,0,
               scale,-scale, scale,   1.0f, 1.0f,   1.0f,0.0f,0.0f,0,
               scale, scale,-scale,   0.0f, 0.0f,   1.0f,0.0f,0.0f,0,
               scale, scale, scale,   0.0f, 1.0f,   1.0f,0.0f,0.0f,0,
        };
    }

    vector<float> getPlaneVertices(float scale, float textureId) {
        return {
             // positions           // uv          // normal
            -scale,-scale, scale,   1.0f, 0.0f,    0.0f,0.0f,1.0f,  textureId,
             scale,-scale, scale,   0.0f, 0.0f,    0.0f,0.0f,1.0f,  textureId,
            -scale, scale, scale,   1.0f, 1.0f,    0.0f,0.0f,1.0f,  textureId,
             scale,-scale, scale,   0.0f, 0.0f,    0.0f,0.0f,1.0f,  textureId,
             scale, scale, scale,   0.0f, 1.0f,    0.0f,0.0f,1.0f,  textureId,
            -scale, scale, scale,   1.0f, 1.0f,    0.0f,0.0f,1.0f,  textureId
        };
    }

    vector<float> getPlaneVertices(float scale, float textureId, Vector3f offset) {
        return {
            // positions           // uv          // normal
           -scale+offset.x,-scale+offset.y, scale+offset.z,   1.0f, 0.0f,    0.0f,0.0f,1.0f,  textureId,
            scale+offset.x,-scale+offset.y, scale+offset.z,   0.0f, 0.0f,    0.0f,0.0f,1.0f,  textureId,
           -scale+offset.x, scale+offset.y, scale+offset.z,   1.0f, 1.0f,    0.0f,0.0f,1.0f,  textureId,
            scale+offset.x,-scale+offset.y, scale+offset.z,   0.0f, 0.0f,    0.0f,0.0f,1.0f,  textureId,
            scale+offset.x, scale+offset.y, scale+offset.z,   0.0f, 1.0f,    0.0f,0.0f,1.0f,  textureId,
           -scale+offset.x, scale+offset.y, scale+offset.z,   1.0f, 1.0f,    0.0f,0.0f,1.0f,  textureId
        };
    }

    MeshData getTilemapModel(float scale, Vector2i size, vector<vector<int>> textureMap) {
        return getTilemapVertices(scale, size, textureMap);
    }

    vector<float> getTilemapVertices(float scale, Vector2i size, vector<vector<int>> textureMap){
        vector<float> map = {};
        for (int col = 0; col < size.y; ++col) {
            vector<float> r = {};
            for (int row = 0; row < size.x; ++row) {
                auto textureId = 0;
                if (col < textureMap.size() && row < textureMap.at(col).size()) {
                    textureId = textureMap.at(col).at(row);
                }
                vector<float> plane = getPlaneVertices(scale, textureId, Vector3f{ row * (scale * 2),-col * (scale * 2),0 });
                r.insert(r.end(), plane.begin(), plane.end());
            }
            map.insert(map.end(), r.begin(), r.end());
        }
        return map;
    }
}