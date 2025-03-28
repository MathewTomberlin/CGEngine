#include "Model.h"
#include "../World/Renderer.h"
#include "../Engine/Engine.h"

namespace CGEngine {
	Model::Model(string sourcePath) : sourcePath(sourcePath) {
		//Import the model using the MeshImporter
		ImportResult importResult = renderer.import(sourcePath);
		if (!importResult.rootNode) {
			cout << "ERROR: Failed to import model from '" << sourcePath << "'\n";
			return;
		}

		//Extract model materials from import result
		modelMaterials = importResult.materials;
		modelAnimations = importResult.animations;
		// Convert ImportResult node tree to ModelNode tree
		rootNode = meshNodeToModelNode(importResult.rootNode, nullptr);
		// Create animator if skeletal
		if (!modelBones.empty()) {
			setupAnimator();
		}
		cout << "\n=== Successfully Imported '" << sourcePath << "' ===\n";
	}

	// Method for manually creating the Model from MeshData
	Model::Model(MeshData* meshData, string name) : sourcePath(name) {
		rootNode = createNode(name, meshData);
		updateBoneData(meshData);
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

	ModelNode* Model::convertAnimationNode(const NodeData& animNode, ModelNode* parent) {
		ModelNode* node = createNode(animNode.name);
		node->localTransform = animNode.transformation;
		node->parent = parent;

		for (const auto& child : animNode.children) {
			ModelNode* childNode = convertAnimationNode(child, node);
			node->children.push_back(childNode);
		}

		return node;
	}

	Model::~Model() {
		delete modelAnimator;

		// Clean up node hierarchy
		cleanupModelNodes(rootNode);

		// Clean up animations
		for (auto& [name, anim] : modelAnimations) {
			delete anim;
		}
	}
	
	id_t Model::instantiate(Transformation3D rootTransform, vector<id_t> overrideMaterials) {
		cout << "\n=== Instantiating Model: '"<< sourcePath <<"' ===\n";

		if (!rootNode) {
			cout << "Cannot instantiate - model has no root node\n";
			return 0;
		}

		//Use override materials if provided, otherwise use model materials
		const vector<id_t>& materialsToUse = overrideMaterials;

		// Ensure we have at least one material
		if (materialsToUse.empty() && modelMaterials.empty()) {
			cout << "No materials available for model instantiation\n";
			modelMaterials.push_back(renderer.getFallbackMaterial()->materialId);
		}

		// Create empty root body first
		id_t rootId = world->create(new Mesh(nullptr));
		Body* rootBody = world->bodies.get(rootId);  // Create empty root
		if (!rootBody) {
			cout << "Failed to create root body\n";
			return 0;
		}

		// Apply root transform to the root body
		rootBody->get<Mesh*>()->setPosition(rootTransform.position);
		rootBody->get<Mesh*>()->setRotation(rootTransform.rotation);
		rootBody->get<Mesh*>()->setScale(rootTransform.scale);

		// Create hierarchy recursively
		bodyCount = 1;
		cout << "  " << bodyCount << ") ROOT (ID:" << rootId << ")\n";
		createChildBodies(rootNode, rootBody, materialsToUse);
		cout << "=== '" << sourcePath << "' Body Count: " << bodyCount << " ===\n";
		return rootId;
	}

	ModelNode* Model::createNode(string name, MeshData* meshData, id_t materialIndex) {
		ModelNode* node = new ModelNode();
		node->nodeName = name;
		node->meshData = meshData;
		node->materialIndex = materialIndex;
		node->localTransform = glm::mat4(1.0f);
		return node;
	}

	void Model::attachNode(ModelNode* parent, ModelNode* child) {
		if (!parent || !child) return;

		child->parent = parent;
		parent->children.push_back(child);

		// Update bone data if needed
		if (child->meshData) {
			updateBoneData(child->meshData);
		}
	}

	ModelNode* Model::copyNodeHierarchy(const ModelNode* source, ModelNode* newParent) {
		if (!source) return nullptr;

		ModelNode* copy = createNode(source->nodeName, source->meshData, source->materialIndex);
		copy->localTransform = source->localTransform;
		copy->parent = newParent;

		for (const auto* childSource : source->children) {
			ModelNode* childCopy = copyNodeHierarchy(childSource, copy);
			copy->children.push_back(childCopy);
		}

		return copy;
	}

	Material* Model::getMaterial(id_t index) {
		if (index < modelMaterials.size()) {
			return world->getMaterial(modelMaterials[index]);
		}
		return nullptr;
	}
	id_t Model::addMaterial(id_t material) {
		modelMaterials.push_back(material);
		return modelMaterials.size() - 1;
	}

	void Model::setNodeMaterial(ModelNode* node, id_t materialIndex) {
		if (node && materialIndex < modelMaterials.size()) {
			node->materialIndex = materialIndex;
		}
	}
	void Model::setMaterial(id_t index, id_t material) {
		if (index >= modelMaterials.size()) {
			modelMaterials.resize(index + 1, -1);
		}
		modelMaterials[index] = material;
	}
	// Add to Animation creation
	void Model::addAnimation(Animation* animation) {
		if (!animation) return;

		// Store the animation
		modelAnimations[animation->getName()] = animation;

		// Update bone data from animation
		for (const auto& [boneName, boneData] : animation->getBoneData()) {
			if (modelBones.find(boneName) == modelBones.end()) {
				modelBones[boneName] = boneData;
			}
		}

		cout << "  Added animation '" << animation->getName() << "' to model '" << sourcePath << "'\n";
	}
	Animation* Model::getAnimation(string animationName) {
		auto it = modelAnimations.find(animationName);
		if (it != modelAnimations.end()) {
			return it->second;
		}
		return nullptr;
	}

	vector<string> Model::getAnimationNames() const {
		vector<string> names;
		for (const auto& [name, animation] : modelAnimations) {
			names.push_back(name);
		}
		return names;
	}

	Animator* Model::createAnimator() const {
		if(!isSkeletal() || modelAnimations.empty()) {
			return nullptr;
		}

		// Create animator with first available animation
		return new Animator(modelAnimations.begin()->second);
	}

	bool Model::isSkeletal() const {
		return !modelBones.empty();
	}

	void Model::mapAnimationNodes(const Animation* anim) {
		if (!anim) return;

		cout << "  Mapping animation '" << anim->getName() << "' nodes to model '" << sourcePath << "' nodes\n";

		// Map animation nodes to model nodes
		mapAnimationNodeRecursive(anim->getRoot(), rootNode);

		// Verify mapping success
		size_t mappedNodes = animationNodeMap.size();
		cout << "    Mapped " << mappedNodes << " nodes for animation\n";

		if (anim->bones.size() > 0 && mappedNodes == 0) {
			cout << "    Warning: No nodes mapped for skeletal animation!\n";
		}
	}

	void Model::mapAnimationNodeRecursive(const NodeData& animNode, ModelNode* modelNode) {
		if (!modelNode) return;

		// Create mapping
		AnimationNodeMapping mapping;
		mapping.modelNode = modelNode;
		mapping.animationNode = const_cast<NodeData*>(&animNode);
		mapping.bindPose = animNode.transformation;
		animationNodeMap[animNode.name] = mapping;

		// Process children
		for (const auto& animChild : animNode.children) {
			for (auto* modelChild : modelNode->children) {
				if (modelChild->nodeName == animChild.name) {
					mapAnimationNodeRecursive(animChild, modelChild);
					break;
				}
			}
		}
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
		vector<Material*> nodeMaterials;
		if (node->meshData) {
			id_t materialId = materials.empty() ? node->materialIndex : materials[0];
			Material* nodeMaterial = world->getMaterial(materialId);
			if (nodeMaterial) {
				nodeMaterials.push_back(nodeMaterial);
				cout << "    - Created Mesh for '" << node->nodeName
					<< "' with Material ID: " << materialId << "\n";
			}
		}

		// Create a mesh for this node (even if empty)
		Mesh* mesh = new Mesh(node->meshData, nodeTransform, nodeMaterials);
		mesh->setModel(this);

		// Create and attach child body
		id_t bodyId = world->create(mesh);
		bodyCount++;
		cout << "  " << bodyCount << ") " << node->nodeName << " (ID:" << bodyId << ")" << (node->meshData && !node->meshData->vertices.empty() ? " <Has Mesh>" : "") << "\n";
		Body* body = world->bodies.get(bodyId);
		if (body) {
			// Attach to parent
			if (parentBody) {
				parentBody->attachBody(body);
			}

			// Set animator if this is a skeletal mesh
			if (isSkeletal() && modelAnimator && node->meshData &&
				!node->meshData->vertices.empty()) {
				mesh->setAnimator(modelAnimator);
			}

			// Process children with this new body as parent
			for (ModelNode* childNode : node->children) {
				createChildBodies(childNode, body, materials);
			}
		} else {
			cout << "ERROR: Failed to create body for node '" << node->nodeName << "'\n";
		}
	}

	// Add new method to handle animation import and setup
	bool Model::setupAnimator() {
		cout << "\n=== Creating Default Animator ===\n";

		// Create single animator instance for the model
		modelAnimator = createAnimator();
		if (!modelAnimator) {
			cout << "ERROR: Failed to create animator\n";
			return false;
		}

		return true;
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

	void Model::setRootNode(ModelNode * node) {
		if (rootNode) delete rootNode;
		rootNode = node;
	}

	void Model::addNode(ModelNode* parent, ModelNode* child) {
		if (!parent || !child) return;
		child->parent = parent;
		parent->children.push_back(child);

		// Update bone data if present
		if (child->meshData) {
			updateBoneData(child->meshData);
		}
	}

	bool Model::validate() const {
		if (!rootNode) return false;
		if (modelMaterials.empty()) return false;
		// Add other validation as needed
		return true;
	}

	vector<Material*> Model::getMaterials() {
		vector<Material*> materials;
		for (id_t materialId : modelMaterials) {
			if (materialId > -1) {
				Material* material = world->getMaterial(materialId);
				if (material) {
					materials.push_back(material);
				}
			}
		}
		return materials;
	}
}