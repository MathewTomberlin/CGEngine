#include "Model.h"
#include "../World/Renderer.h"
#include "../Engine/Engine.h"

namespace CGEngine {
	Model::Model(string sourcePath, const string& skeletonName) : sourcePath(sourcePath) {
		init();
		//Import the model using the MeshImporter
		log(this, LogInfo, "Importing Model: {}", sourcePath);
		ImportResult importResult = renderer.import(sourcePath, skeletonName);
		if (!importResult.rootNode) {
			log(this, LogError, "Failed to import model from '{}'", sourcePath);
			return;
		}

		//Extract model materials from import result
		modelMaterials = importResult.materials;
		modelAnimations = importResult.animations;
		// Convert ImportResult node tree to ModelNode tree and collect modelBones from node bones
		rootNode = meshNodeToModelNode(importResult.rootNode, nullptr);
		//If the model has a skeleton, get it and try to create an animator
		if (importResult.skeleton) {
			modelSkeleton = importResult.skeleton;
			// Create animator if skeletal
			if (modelSkeleton && modelSkeleton->isValid()) {
				modelAnimator = createAnimator();
				if (!modelAnimator) {
					log(this, LogError, "Failed to create animator");
				}
			}
		}
		log(this, LogInfo, "Successfully Imported '{}'", sourcePath);
	}

	// Method for manually creating the Model from MeshData
	Model::Model(MeshData* meshData, string name, string skeletonName) : sourcePath(name) {
		init();
		rootNode = createNode(name, meshData);
		updateBoneData(meshData);
		//If the model has bones, create a skeleton and animator
		if (modelBones.size() > 0) {
			//If skeletonName is empty or if that skeleton exists but doesn't have matching bones, use a name that will create a new skeleton
			string newSkeletonName = (skeletonName.empty() || (!skeletonName.empty() && !assets.get<Skeleton>(skeletonName)->equals(modelBones))) ? name.append("_Skeleton") : skeletonName;
			//Get the Skeleton with newSkeletonName, if it exists, or create a Skeleton from model bones
			optional<id_t> skeletonId = assets.create<Skeleton>(newSkeletonName, modelBones);
			if (skeletonId.has_value()) {
				modelSkeleton = assets.get<Skeleton>(skeletonId.value());
				//Create animator if skeletal
				if (modelSkeleton && modelSkeleton->isValid()) {
					modelAnimator = createAnimator();
					if (!modelAnimator) {
						log(this, LogError, "Failed to create animator");
					}
				}
			}
		}
	}

	ModelNode* Model::meshNodeToModelNode(MeshNodeData * meshNode, ModelNode * modelNode) {
		if (!meshNode) return nullptr;

		//Map the Assimp node data to Model node data
		ModelNode* node = new ModelNode();
		node->nodeName = meshNode->nodeName;
		node->meshData = meshNode->meshData;
		node->parent = modelNode;
		node->localTransform = meshNode->transformation;
		node->materialIndex = meshNode->materialId;

		// Update bone data if this node has a mesh with bones
		if (node->meshData && !node->meshData->bones.empty()) {
			updateBoneData(node->meshData);
		}

		// Convert children
		for (auto* childImport : meshNode->children) {
			ModelNode* childNode = meshNodeToModelNode(childImport, node);
			if (childNode) {
				node->children.push_back(childNode);
			}
		}

		return node;
	}

	//Method to update the Model bone data from MeshData
	void Model::updateBoneData(const MeshData* meshData) {
		if (!meshData) return;

		// Merge bone data from MeshData to Model
		for (const auto& [boneName, boneData] : meshData->bones) {
			modelBones[boneName] = boneData;
		}
	}

	void Model::setSkeleton(Skeleton* skeleton) { 
		this->modelSkeleton = skeleton; 
	}

	Model::~Model() {
		delete modelAnimator;

		// Clean up node hierarchy
		cleanupModelNodes(rootNode);

		modelAnimations.clear();
	}
	
	optional<id_t> Model::instantiate(Transformation3D rootTransform, vector<id_t> overrideMaterials) {
		log(this, LogInfo, "Instantiating Model: '{}'", sourcePath);

		if (!rootNode) {
			log(this, LogError, "Failed to instantiate model with root node");
			return 0;
		}

		//Use override materials if provided, otherwise use model materials
		const vector<id_t>& materialsToUse = overrideMaterials;

		// Ensure we have at least one material
		if (materialsToUse.empty() && modelMaterials.empty()) {
			log(this, LogWarn, "No materials to use during instantiation. Using fallback material.");
			modelMaterials.push_back(renderer.getFallbackMaterial()->materialId); //TODO: Ensure renderer fallback uses AssetManager
		}

		// Create null Mesh Body root
		optional<id_t> rootId = assets.create<Body>(sourcePath.append(".Root"), new Mesh(nullptr));
		if (rootId.has_value()) {
			Body* rootBody = assets.get<Body>(rootId.value());

			//Apply root body as the first Model body
			bodyCount = 1;
			log(this, LogDebug, "  {}) ROOT (ID: {})", bodyCount, rootId);

			// Apply root transform to the root mesh
			Mesh* rootMesh = rootBody->get<Mesh*>();
			rootMesh->setPosition(rootTransform.position);
			rootMesh->setRotation(rootTransform.rotation);
			rootMesh->setScale(rootTransform.scale);

			//Recursively build Model hierarchy
			createChildBodies(rootNode, rootBody, materialsToUse);
			log(this, LogInfo, "Successfully Instantiated Model '{}' with Body Count: {}", sourcePath, bodyCount);
			return rootId;
		} else {
			log(this, LogError, " Failed to create root body");
			return nullopt;
		}
	}

	ModelNode* Model::createNode(string name, MeshData* meshData, id_t materialIndex) {
		ModelNode* node = new ModelNode();
		node->nodeName = name;
		node->meshData = meshData;
		node->materialIndex = materialIndex;
		node->localTransform = glm::mat4(1.0f);
		return node;
	}

	Animator* Model::createAnimator() const {
		if(!modelSkeleton || modelAnimations.empty()) return nullptr;

		//Animator is created with the first animation imported
		string firstAnimationName = *modelAnimations.begin();
		Animator* animator = new Animator(firstAnimationName);
		//Animator's skeleton is set to the Model's skeleton
		animator->setSkeleton(modelSkeleton);
		return animator;
	}

	bool Model::isSkeletal() const {
		return !modelBones.empty();
	}

	// Recursively create child bodies from ModelNode hierarchy
	void Model::createChildBodies(ModelNode* node, Body* parentBody, const vector<id_t>& materials) {
		// Convert node transform to Transformation3D
		glm::vec3 translation, scale, skew;
		glm::quat rotation;
		glm::vec4 perspective;
		glm::decompose(node->localTransform, scale, rotation, translation, skew, perspective);
		Transformation3D nodeTransform(
			Vector3f(translation.x, translation.y, translation.z),
			Vector3f(renderer.fromGlm(glm::degrees(glm::eulerAngles(rotation)))),
			Vector3f(scale.x, scale.y, scale.z)
		);

		// Create material vector for this node
		vector<id_t> nodeMaterials;
		if (node->meshData) {
			id_t materialId = materials.empty() ? node->materialIndex : materials[0];
			nodeMaterials.push_back(materialId);
			log(this, LogDebug, "      - Created Mesh for '{}' with Material ID: {}", node->nodeName, materialId);
		}

		// Create a mesh for this node (even if empty)
		Mesh* mesh = new Mesh(node->meshData, nodeTransform, nodeMaterials);
		mesh->setModelId(getId());

		// Create and attach child body
		optional<id_t> bodyId = assets.create<Body>(sourcePath.append(node->nodeName), mesh);
		if (bodyId.has_value()) {
			Body* body = assets.get<Body>(bodyId.value());
			bodyCount++;
			log(this, LogDebug, "  {}) {} (ID: {}) {}", bodyCount, node->nodeName, bodyId, node->meshData && !node->meshData->vertices.empty() ? " <Has Mesh>" : "");
			//Attach child body to parent
			if (parentBody) {
				parentBody->attachBody(body);
			}

			//Process children with this new body as parent
			for (ModelNode* childNode : node->children) {
				createChildBodies(childNode, body, materials);
			}
		} else {
			log(this, LogError, "Failed to create body for node '{}'", node->nodeName);
		}
	}

	//Recursively clean up ModelNode hierarchy
	void Model::cleanupModelNodes(ModelNode* node) {
		if (!node) return;

		auto children = node->children;
		for (ModelNode* child : children) {
			cleanupModelNodes(child);
		}

		delete node;
	}

	vector<Material*> Model::getMaterials() {
		vector<Material*> materials;
		for (id_t materialId : modelMaterials) {
			if (materialId > -1) {
				Material* material = assets.get<Material>(materialId);
				if (material) {
					materials.push_back(material);
				}
			}
		}
		return materials;
	}

	bool Model::isValid() const {
		return true;
	}
}