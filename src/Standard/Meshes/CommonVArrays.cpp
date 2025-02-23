#include "CommonVArrays.h"

namespace CGEngine {
    VertexModel getCubeModel(float scale) {
        return VertexModel(getCubeVertices(scale));
    }

    VertexModel getPlaneModel(float scale) {
        //TODO: Fix plane indices
        return VertexModel(getPlaneVertices(scale));
    }

    vector<float> getCubeVertices(float scale) {
        return {
            // positions            // texture coordinates  // normals
            //bottom
            -scale,-scale,-scale,   0.0f, 0.0f,             0.0f,-1.0f,0.0f,
             scale,-scale,-scale,   1.0f, 0.0f,             0.0f,-1.0f,0.0f,
            -scale,-scale, scale,   0.0f, 1.0f,             0.0f,-1.0f,0.0f,
             scale,-scale,-scale,   1.0f, 0.0f,             0.0f,-1.0f,0.0f,
             scale,-scale, scale,   1.0f, 1.0f,             0.0f,-1.0f,0.0f,
            -scale,-scale, scale,   0.0f, 1.0f,             0.0f,-1.0f,0.0f,

            // top
            -scale, scale,-scale,   0.0f, 0.0f,             0.0f,1.0f,0.0f,
            -scale, scale, scale,   0.0f, 1.0f,             0.0f,1.0f,0.0f,
             scale, scale,-scale,   1.0f, 0.0f,             0.0f,1.0f,0.0f,
             scale, scale,-scale,   1.0f, 0.0f,             0.0f,1.0f,0.0f,
            -scale, scale, scale,   0.0f, 1.0f,             0.0f,1.0f,0.0f,
             scale, scale, scale,   1.0f, 1.0f,             0.0f,1.0f,0.0f,

             // front
             -scale,-scale, scale,   1.0f, 0.0f,            0.0f,0.0f,1.0f,
              scale,-scale, scale,   0.0f, 0.0f,            0.0f,0.0f,1.0f,
             -scale, scale, scale,   1.0f, 1.0f,            0.0f,0.0f,1.0f,
              scale,-scale, scale,   0.0f, 0.0f,            0.0f,0.0f,1.0f,
              scale, scale, scale,   0.0f, 1.0f,            0.0f,0.0f,1.0f,
             -scale, scale, scale,   1.0f, 1.0f,            0.0f,0.0f,1.0f,

             // back
             -scale,-scale,-scale,   0.0f, 0.0f,            0.0f,0.0f,-1.0f,
             -scale, scale,-scale,   0.0f, 1.0f,            0.0f,0.0f,-1.0f,
              scale,-scale,-scale,   1.0f, 0.0f,            0.0f,0.0f,-1.0f,
              scale,-scale,-scale,   1.0f, 0.0f,            0.0f,0.0f,-1.0f,
             -scale, scale,-scale,   0.0f, 1.0f,            0.0f,0.0f,-1.0f,
              scale, scale,-scale,   1.0f, 1.0f,            0.0f,0.0f,-1.0f,

              // left
              -scale,-scale, scale,   0.0f, 1.0f,           -1.0f,0.0f,0.0f,
              -scale, scale,-scale,   1.0f, 0.0f,           -1.0f,0.0f,0.0f,
              -scale,-scale,-scale,   0.0f, 0.0f,           -1.0f,0.0f,0.0f,
              -scale,-scale, scale,   0.0f, 1.0f,           -1.0f,0.0f,0.0f,
              -scale, scale, scale,   1.0f, 1.0f,           -1.0f,0.0f,0.0f,
              -scale, scale,-scale,   1.0f, 0.0f,           -1.0f,0.0f,0.0f,

              // right
               scale,-scale, scale,   1.0f, 1.0f,           1.0f,0.0f,0.0f,
               scale,-scale,-scale,   1.0f, 0.0f,           1.0f,0.0f,0.0f,
               scale, scale,-scale,   0.0f, 0.0f,           1.0f,0.0f,0.0f,
               scale,-scale, scale,   1.0f, 1.0f,           1.0f,0.0f,0.0f,
               scale, scale,-scale,   0.0f, 0.0f,           1.0f,0.0f,0.0f,
               scale, scale, scale,   0.0f, 1.0f,           1.0f,0.0f,0.0f
        };
    }

    vector<float> getCubeNormals() {
        return {
            0,-1,0,
            0,-1,0,
            0,-1,0,
            0,-1,0,
            0,-1,0,
            0,-1,0,

            0,1,0,
            0,1,0,
            0,1,0,
            0,1,0,
            0,1,0,
            0,1,0,

            0,0,1,
            0,0,1,
            0,0,1,
            0,0,1,
            0,0,1,
            0,0,1,

            0,0,-1,
            0,0,-1,
            0,0,-1,
            0,0,-1,
            0,0,-1,
            0,0,-1,

            -1,0,0,
            -1,0,0,
            -1,0,0,
            -1,0,0,
            -1,0,0,
            -1,0,0,

            1,0,0,
            1,0,0,
            1,0,0,
            1,0,0,
            1,0,0,
            1,0,0
        };
    }

    vector<unsigned> getCubeIndices() {
        return {
            2, 1, 0, 3, 2, 0, //front
            4, 5, 6, 4, 6, 7, //right
            8, 9, 10, 8, 10, 11, //back
            14, 13, 12, 15, 14, 12, //left
            16, 17, 18, 16, 18, 19, //upper
            22, 21, 20, 23, 22, 20  //bottom
        };
    }

    vector<float> getPlaneVertices(float scale) {
        return {
             // positions    // texture coordinates
            - scale, -scale,  0,  0, 0,
             scale, -scale,  0,  1, 0,
            -scale,  scale,  0,  0, 1,
            -scale,  scale,  0,  0, 1,
             scale, -scale,  0,  1, 0,
             scale,  scale,  0,  1, 1
        };
    }
}