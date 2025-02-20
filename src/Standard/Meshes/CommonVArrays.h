#pragma once

#include "SFML/OpenGL.hpp"
#include <vector>
using namespace std;

namespace CGEngine {
	extern vector<GLfloat> getCubeVertices(float scale);
	extern vector<GLfloat> getCubeNormals();
	extern vector<GLfloat> getPlaneVertices(float scale);
}