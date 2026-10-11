#include "RuntimeMesh.h"
#include "../Engine/Engine.h"
#include "../Body/Body.h"
#include "Mesh.h"

namespace CGEngine {
	Body* findBodyByName(const std::string& name) {
		for (Body* body : assets.getAllResources<Body>()) {
			if (body->getName() == name) return body;
		}
		return nullptr;
	}

	Body* createRuntimeMeshBody(const std::string& name, std::vector<VertexData> vertices, std::vector<unsigned int> indices,
		const std::vector<id_t>& materials, Vector3f position) {
		// Each runtime mesh gets its own data under a unique asset name, so no two bodies share it.
		static size_t runtimeMeshCount = 0;
		auto meshData = assets.create<MeshData>("runtime:" + name + "#" + std::to_string(++runtimeMeshCount),
			std::move(vertices), std::move(indices));
		if (!meshData.has_value()) return nullptr;
		meshData.value().second->runtimeMesh = true;

		Mesh mesh(meshData.value().second, Transformation3D(position), materials);
		// Body asset names are cache keys. A unique key per body stops a re-created body returning an old one.
		static size_t runtimeBodyCount = 0;
		auto body = assets.create<Body>("body:" + name + "#" + std::to_string(++runtimeBodyCount), mesh);
		if (!body.has_value()) return nullptr;
		body.value().second->setName(name);
		return body.value().second;
	}

	bool updateRuntimeMeshBody(Body* body, std::vector<VertexData> vertices, std::vector<unsigned int> indices) {
		Mesh* mesh = body ? body->get<Mesh*>() : nullptr;
		MeshData* meshData = mesh ? mesh->getMeshData() : nullptr;
		if (!meshData || !meshData->runtimeMesh) return false;
		meshData->vertices = std::move(vertices);
		meshData->indices = std::move(indices);
		meshData->boundsValid = false;
		meshData->gpuDirty = true;
		return true;
	}

	bool removeRuntimeMeshBody(const std::string& name) {
		Body* body = findBodyByName(name);
		if (!body || body == world->getRoot()) return false;
		Mesh* mesh = body->get<Mesh*>();
		MeshData* meshData = mesh ? mesh->getMeshData() : nullptr;
		body->deleteBody(ChildrenTermination::Terminate);
		// Runtime mesh data belongs to its body alone: free the GL objects, then the asset.
		if (meshData && meshData->runtimeMesh) {
			renderer->releaseMeshData(meshData);
			if (auto id = meshData->getId()) assets.remove<MeshData>(id.value());
		}
		return true;
	}
}
