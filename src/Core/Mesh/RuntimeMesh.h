#pragma once

#include <string>
#include <vector>
#include "../World/Renderer.h"
#include "../Types/Types.h"

namespace CGEngine {
	class Body;

	// Mesh bodies whose vertices come from code: from Python (create_mesh_body) or the voxel mesher. The mesh data
	// belongs to its body alone, so removing the body frees it too.
	Body* createRuntimeMeshBody(const std::string& name, std::vector<VertexData> vertices, std::vector<unsigned int> indices,
		const std::vector<id_t>& materials, Vector3f position);

	// Replaces the vertices and indices of a runtime mesh body. The GPU copy is updated before the next frame.
	// Returns false if the body is not a runtime mesh.
	bool updateRuntimeMeshBody(Body* body, std::vector<VertexData> vertices, std::vector<unsigned int> indices);

	// Removes the body with this name and frees its runtime mesh. False if there is no such body (or it is the root).
	bool removeRuntimeMeshBody(const std::string& name);

	// The first body with this name, or nullptr.
	Body* findBodyByName(const std::string& name);
}
