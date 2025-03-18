#pragma once

#include "../../Core/Mesh/Mesh.h"
#include <vector>
using namespace std;

namespace CGEngine {
	extern MeshData getCubeModel(float scale);
	vector<VertexData> getCubeVertices(float scale);
	vector<unsigned int> getCubeIndices();
	extern MeshData getPlaneModel(float scale, float textureId, Vector3f offset = { 0,0,0 });
	vector<VertexData> getPlaneVertices(float scale, float textureId);
	vector<VertexData> getPlaneVertices(float scale, float textureId, Vector3f offset={0,0,0});
	vector<unsigned int> getPlaneIndices();
	extern MeshData getTilemapModel(float scale, Vector2i size, vector<vector<int>> textureMap = {});
	vector<VertexData> getTilemapVertices(float scale, Vector2i size, vector<vector<int>> textureMap={});
	vector<unsigned int> getTilemapIndices(Vector2i size);
}