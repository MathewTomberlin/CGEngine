#include "RuntimeMesh.h"
#include "../Engine/Engine.h"
#include "../Body/Body.h"
#include "Mesh.h"
#include <unordered_map>

namespace CGEngine {
	namespace {
		// Adds a body with this mesh data. The body's name is unique per call, because asset names are cache keys.
		Body* attachMesh(const std::string& name, MeshData* meshData, const std::vector<id_t>& materials, Vector3f position) {
			Mesh mesh(meshData, Transformation3D(position), materials);
			static size_t runtimeBodyCount = 0;
			auto body = assets.create<Body>("body:" + name + "#" + std::to_string(++runtimeBodyCount), mesh);
			if (!body.has_value()) return nullptr;
			body.value().second->setName(name);
			return body.value().second;
		}

		// A runtime mesh's data has its own asset name, so no two bodies share it.
		std::string runtimeMeshName(const std::string& name) {
			static size_t runtimeMeshCount = 0;
			return "runtime:" + name + "#" + std::to_string(++runtimeMeshCount);
		}
	}

	Body* findBodyByName(const std::string& name) {
		// The creatures' scripts ask for their bodies by name every frame, and a scan of every body each time is what that cost. The id
		// of the body found is kept; it is used only while the body is still there under that name (an id can be reused by another body).
		static std::unordered_map<std::string, id_t> idByName;
		auto cached = idByName.find(name);
		if (cached != idByName.end()) {
			Body* body = assets.get<Body>(cached->second);
			if (body && body->getName() == name) return body;
		}
		for (Body* body : assets.getAllResources<Body>()) {
			if (body->getName() != name) continue;
			if (auto id = body->getId()) idByName[name] = id.value();
			return body;
		}
		idByName.erase(name);
		return nullptr;
	}

	Body* createRuntimeMeshBody(const std::string& name, std::vector<VertexData> vertices, std::vector<unsigned int> indices,
		const std::vector<id_t>& materials, Vector3f position) {
		auto meshData = assets.create<MeshData>(runtimeMeshName(name), std::move(vertices), std::move(indices));
		if (!meshData.has_value()) return nullptr;
		meshData.value().second->runtimeMesh = true;
		return attachMesh(name, meshData.value().second, materials, position);
	}

	Body* createCompactMeshBody(const std::string& name, std::vector<VoxelVertex> vertices, std::vector<unsigned int> indices,
		const std::vector<id_t>& materials, Vector3f position) {
		auto meshData = assets.create<MeshData>(runtimeMeshName(name), std::move(vertices), std::move(indices));
		if (!meshData.has_value()) return nullptr;
		meshData.value().second->runtimeMesh = true;
		Body* made = attachMesh(name, meshData.value().second, materials, position);
		return made;
	}

	namespace {
		// The runtime mesh of a body, if it has one of the given layout. The layout is fixed when the mesh is made, since
		// the GPU's attribute setup depends on it.
		MeshData* runtimeMeshOf(Body* body, bool compact) {
			Mesh* mesh = body ? body->get<Mesh*>() : nullptr;
			MeshData* meshData = mesh ? mesh->getMeshData() : nullptr;
			return meshData && meshData->runtimeMesh && meshData->compact == compact ? meshData : nullptr;
		}
	}

	bool updateRuntimeMeshBody(Body* body, std::vector<VertexData> vertices, std::vector<unsigned int> indices) {
		MeshData* meshData = runtimeMeshOf(body, false);
		if (!meshData) return false;
		meshData->vertices = std::move(vertices);
		meshData->indices = std::move(indices);
		meshData->boundsValid = false;
		meshData->gpuDirty = true;
		return true;
	}

	bool updateCompactMeshBody(Body* body, std::vector<VoxelVertex> vertices, std::vector<unsigned int> indices) {
		MeshData* meshData = runtimeMeshOf(body, true);
		if (!meshData) return false;
		meshData->voxelVertices = std::move(vertices);
		meshData->indices = std::move(indices);
		meshData->arraysFreed = false;
		meshData->boundsValid = false;
		meshData->gpuDirty = true;
		return true;
	}

	bool removeRuntimeMeshBody(const std::string& name) {
		return removeRuntimeMesh(findBodyByName(name));
	}

	bool removeRuntimeMesh(Body* body) {
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
