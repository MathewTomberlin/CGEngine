#pragma once

#include "../../Core/Mesh/Mesh.h"
#include <vector>
using namespace std;

namespace CGEngine {
	extern MeshData getCubeModel(float scale);
	vector<float> getCubeVertices(float scale);
	extern MeshData getPlaneModel(float scale, float textureId, Vector3f offset = { 0,0,0 });
	vector<float> getPlaneVertices(float scale, float textureId);
	vector<float> getPlaneVertices(float scale, float textureId, Vector3f offset={0,0,0});
	extern MeshData getTilemapModel(float scale, Vector2i size, vector<vector<int>> textureMap = {});
	vector<float> getTilemapVertices(float scale, Vector2i size, vector<vector<int>> textureMap={});
}