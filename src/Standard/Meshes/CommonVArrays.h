#pragma once

#include "../../Core/Mesh/Mesh.h"
#include <vector>
using namespace std;

namespace CGEngine {
	extern VertexModel getCubeModel(float scale);
	vector<float> getCubeVertices(float scale);
	vector<float> getCubeNormals();
	vector<unsigned> getCubeIndices();
	extern VertexModel getPlaneModel(float scale);
	vector<float> getPlaneVertices(float scale);
}