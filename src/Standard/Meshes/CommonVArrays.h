#pragma once

#include "SFML/OpenGL.hpp"
#include <vector>
#include "../../Core/Mesh/Mesh.h"
using namespace std;

namespace CGEngine {
	extern VertexModel getCubeModel(float scale);
	vector<GLfloat> getCubeVertices(float scale);
	vector<GLfloat> getCubeNormals();
	extern VertexModel getPlaneModel(float scale);
	vector<GLfloat> getPlaneVertices(float scale);
}