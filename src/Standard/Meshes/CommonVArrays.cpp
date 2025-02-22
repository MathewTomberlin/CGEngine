#include "CommonVArrays.h"

namespace CGEngine {
    VertexModel getCubeModel(float scale) {
        return VertexModel(getCubeVertices(scale), getCubeIndices(), getCubeNormals());
    }

    VertexModel getPlaneModel(float scale) {
        //TODO: Fix plane indices
        return VertexModel(getPlaneVertices(scale), {});
    }

    vector<float> getCubeVertices(float scale) {
        return {
            // positions   // texture coordinates
           //front
           -scale, -scale, -scale,  0, 0,
            scale, -scale, -scale,  1, 0,
            scale,  scale, -scale,  1, 1,
           -scale,  scale, -scale,  0, 1,
           //right
            scale,  scale, -scale,  1, 1,
            scale,  scale,  scale,  0, 1,
            scale, -scale,  scale,  0, 0,
            scale, -scale, -scale,  1, 0,
            //back
            -scale, -scale,  scale,  0, 0,
             scale, -scale,  scale,  1, 0,
             scale,  scale,  scale,  1, 1,
            -scale,  scale,  scale,  0, 1,
            //left
            -scale, -scale,  scale,  0, 0,
            -scale, -scale, -scale,  1, 0,
            -scale,  scale, -scale,  1, 1,
            -scale,  scale,  scale,  0, 1,
            //upper
             scale, -scale,  scale,  0, 1,
            -scale, -scale,  scale,  1, 1,
            -scale, -scale, -scale,  1, 0,
             scale, -scale, -scale,  0, 0,
             //bottom
             -scale,  scale, -scale,  0, 1,
              scale,  scale, -scale,  1, 1,
              scale,  scale,  scale,  1, 0,
             -scale,  scale,  scale,  0, 0,
        };
    }

    vector<float> getCubeNormals() {
        return {
            0,0,-1,
            0,0,-1,
            0,0,-1,
            0,0,-1,
            0,0,-1,
            0,0,-1,

            1,0,0,
            1,0,0,
            1,0,0,
            1,0,0,
            1,0,0,
            1,0,0,

            0,0,1,
            0,0,1,
            0,0,1,
            0,0,1,
            0,0,1,
            0,0,1,

            -1,0,0,
            -1,0,0,
            -1,0,0,
            -1,0,0,
            -1,0,0,
            -1,0,0,

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
            0,1,0
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