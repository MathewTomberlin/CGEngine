#pragma once

#include "../../Core/Mesh/Mesh.h"
#include <vector>
using namespace std;

namespace CGEngine {
	extern MeshData* getCubeModel(float scale);
	vector<VertexData> getCubeVertices(float scale);
	vector<unsigned int> getCubeIndices();
	extern MeshData* getPlaneModel(float scale, float textureId, Vector3f offset = { 0,0,0 });
	vector<VertexData> getPlaneVertices(float scale, float textureId);
	vector<VertexData> getPlaneVertices(float scale, float textureId, Vector3f offset={0,0,0});
	vector<unsigned int> getPlaneIndices();
	// A UV sphere of the given radius, centred on the origin, with normals pointing out. segments around, rings from pole to pole.
	vector<VertexData> getSphereVertices(float radius, int segments = 16, int rings = 12);
	vector<unsigned int> getSphereIndices(int segments = 16, int rings = 12, const vector<VertexData>& vertices = {});
	extern MeshData* getTilemapModel(float scale, Vector2i size, vector<vector<int>> textureMap = {});
	vector<VertexData> getTilemapVertices(float scale, Vector2i size, vector<vector<int>> textureMap={});
	vector<unsigned int> getTilemapIndices(Vector2i size);
}