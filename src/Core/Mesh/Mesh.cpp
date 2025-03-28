#include "Mesh.h"

namespace CGEngine {
	Mesh::Mesh(MeshData* meshData, Transformation3D transformation, vector<Material*> materials, RenderParameters renderParams, string importPath) : meshData(meshData), transformation(transformation), renderParameters(renderParams), materials(materials), importPath(importPath), animator(nullptr) {
		if (!importPath.empty()) {
			import(importPath);
		}
		renderer.getModelData(this);
	};

	//TODO: ImportResult meshes should be in a tree, not a vector
	//TODO: ImportResult meshes should be SubMesh structs of MeshData and Material Id. Should Material Id be world id or id among materials on mesh?
	//TODO: Assign imported submesh mesh its correct material. Only assign root mesh its correct material?
	void Mesh::import(string importPath) {
		clearMaterials();
		ImportResult importResult = renderer.import(importPath);
		if (importResult.rootNode && importResult.rootNode->meshData) {
			cout << "Setting up mesh successfully imported from '" << importPath << "'\n";
			this->meshData = importResult.rootNode->meshData;
			this->meshData->sourcePath = importPath;
			if (importResult.rootNode->hasMesh() && !importResult.rootNode->meshData->bones.empty()) {
				cout << "Setting up Skeletal Mesh!\n";
			}
			// Set root transformation
			glm::vec3 translation, scale, skew;
			glm::quat rotation;
			glm::vec4 perspective;
			glm::decompose(importResult.rootNode->transformation, scale, rotation, translation, skew, perspective);

			this->transformation = Transformation3D(
				Vector3f(translation.x, translation.y, translation.z),
				Vector3f(renderer.fromGlm(glm::degrees(glm::eulerAngles(rotation)))),
				Vector3f(scale.x, scale.y, scale.z)
			);

			vector<MeshNodeData*> nodes = importResult.rootNode->children;
			while (nodes.size() > 0) {
				MeshNodeData* node = nodes.back();
				nodes.pop_back();
				if (node->meshData) {
					glm::decompose(node->transformation, scale, rotation, translation, skew, perspective);

					Vector3f pos(translation.x, translation.y, translation.z);
					Vector3f rot = renderer.fromGlm(glm::degrees(glm::eulerAngles(rotation)));
					Vector3f scl(scale.x, scale.y, scale.z);

					Mesh* nodeMesh = new Mesh(node->meshData, Transformation3D(pos, rot, scl), { world->getMaterial(node->materialId) }, renderParameters);
					nodeMesh->meshData->sourcePath = importPath;
					id_t childMeshBodyId = world->create(nodeMesh);
					Body* childBody = world->bodies.get(childMeshBodyId);
					if (childBody) {
						world->getRoot()->attachBody(childBody);
					}
				}
				nodes.insert(nodes.end(), node->children.begin(), node->children.end());
			}

			deleteImportHeirarchy(importResult.rootNode);
		}
	}

	void Mesh::deleteImportHeirarchy(MeshNodeData* node) {
		if (!node) return;
		for (MeshNodeData* child : node->children) {
			deleteImportHeirarchy(child);
		}
		delete node;
	}

	MeshData* Mesh::getMeshData() {
		return meshData;
	}

	void Mesh::setMeshData(MeshData* model) {
		this->meshData = model;
	}

	void Mesh::render(Transform transform) {
		renderer.pullGL();

		//Combine SFML entity transform components with 3D transformation components
		Vector2f position2d = world->getGlobalPosition(transform);
		Vector3f position = { position2d.x + transformation.position.x,position2d.y + transformation.position.y,transformation.position.z };

		Angle rotation2d = world->getGlobalRotation(transform);
		Vector3f rotation = { transformation.rotation.x,transformation.rotation.y, rotation2d.asDegrees()+transformation.rotation.z, };

		Vector2f scale2d = world->getGlobalScale(transform);
		Vector3f scale = { scale2d.x * transformation.scale.x,scale2d.y * transformation.scale.y, transformation.scale.z };
		Transformation3D combinedTransformation = Transformation3D(position, rotation, scale);

		renderer.renderMesh(this, meshData, combinedTransformation);
		renderer.commitGL();
	}

	void Mesh::bindTexture(Texture* texture) {
		if (renderer.setGLWindowState(true)) {
			//Generate texture mipmaps and bind or clear
			if (texture != nullptr) {
				(void)texture->generateMipmap();
				Texture::bind(&(*texture));
			}
			else {
				Texture::bind(nullptr);
			}
			renderer.setGLWindowState(false);
		}
	}

	void Mesh::setPosition(Vector3f pos) {
		transformation.position = pos;
	}
	void Mesh::move(Vector3f delta) {
		transformation.position += delta;
	}

	void Mesh::setRotation(Vector3f rot) {
		transformation.rotation = rot;
	}

	void Mesh::rotate(Vector3f delta) {
		transformation.rotation += delta;
	}

	void Mesh::setScale(Vector3f scale) {
		transformation.scale = scale;
	}

	void Mesh::scale(Vector3f delta) {
		transformation.scale += delta;
	}

	vector<Material*> Mesh::getMaterials() {
		return materials;
	}

	id_t Mesh::addMaterial(Material* material) {
		id_t id = materials.size();
		materials.push_back(material);
		return id;
	}

	void Mesh::clearMaterials() {
		materials.clear();
	}

	void Mesh::setImportPath(string path) {
		importPath = path;
	}
	string Mesh::getImportPath() {
		return importPath;
	}

	void Mesh::setAnimator(Animator* animator) {
		this->animator = animator;
	}
	Animator* Mesh::getAnimator() {
		return animator;
	}

	string Mesh::getMeshName() const { return meshData ? meshData->meshName : ""; }
	string Mesh::getSourcePath() const { return meshData ? meshData->sourcePath : ""; }
}