#include "CommonVArrays.h"

namespace CGEngine {
    VertexModel getCubeModel(float scale) {
        return VertexModel(getCubeVertices(scale), getCubeNormals());
    }

    VertexModel getPlaneModel(float scale) {
        return VertexModel(getPlaneVertices(scale));
    }

    vector<GLfloat> getCubeVertices(float scale) {
        return {
            // positions    // texture coordinates
            -scale, -scale, -scale*2,  0, 0,
            -scale,  scale, -scale*2,  1, 0,
            -scale, -scale,  0,  0, 1,
            -scale, -scale,  0,  0, 1,
            -scale,  scale, -scale * 2,  1, 0,
            -scale,  scale,  0,  1, 1,

             scale, -scale, -scale*2,  0, 0,
             scale,  scale, -scale*2,  1, 0,
             scale, -scale,  0,  0, 1,
             scale, -scale,  0,  0, 1,
             scale,  scale, -scale * 2,  1, 0,
             scale,  scale,  0,  1, 1,

            -scale, -scale, -scale*2,  0, 0,
             scale, -scale, -scale*2,  1, 0,
            -scale, -scale,  0,  0, 1,
            -scale, -scale,  0,  0, 1,
             scale, -scale, -scale * 2,  1, 0,
             scale, -scale,  0,  1, 1,

            -scale,  scale, -scale*2,  0, 0,
             scale,  scale, -scale*2,  1, 0,
            -scale,  scale,  0,  0, 1,
            -scale,  scale,  0,  0, 1,
             scale,  scale, -scale * 2,  1, 0,
             scale,  scale,  0,  1, 1,

            -scale, -scale, -scale*2,  0, 0,
             scale, -scale, -scale*2,  1, 0,
            -scale,  scale, -scale*2,  0, 1,
            -scale,  scale, -scale*2,  0, 1,
             scale, -scale, -scale*2,  1, 0,
             scale,  scale, -scale*2,  1, 1,

            -scale, -scale,  0,  0, 0,
             scale, -scale,  0,  1, 0,
            -scale,  scale,  0,  0, 1,
            -scale,  scale,  0,  0, 1,
             scale, -scale,  0,  1, 0,
             scale,  scale,  0,  1, 1
        };
    }

    vector<GLfloat> getCubeNormals() {
        return {
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
            1,0,0,

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

            0,0,-1,
            0,0,-1,
            0,0,-1,
            0,0,-1,
            0,0,-1,
            0,0,-1,

            0,0,1,
            0,0,1,
            0,0,1,
            0,0,1,
            0,0,1,
            0,0,1
        };
    }

    vector<GLfloat> getPlaneVertices(float scale) {
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