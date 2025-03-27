#include "Model.h"
#include "../World/Renderer.h"
#include "../Engine/Engine.h"

namespace CGEngine {
	Model::Model(string sourcePath) : sourcePath(sourcePath) {
		// Import the model using MeshImporter
		ImportResult importResult = renderer.import(sourcePath);
		if (!importResult.rootNode) {
			cout << "ERROR: Failed to import model from '" << sourcePath << "'\n";
			return;
		}

		//TODO: I believe I can combine these two lambdas, as they both seem to visit all nodes in the imported tree
		
		// Create materials
		// Track materials we've already created to avoid duplicates
		bool hasBones = false;
		map<id_t, id_t> materialIdMap;  // originalId -> newId
		// Function to process materials recursively
		function<void(MeshNodeData*)> processMaterials = [&](MeshNodeData* node) {
			if (node) {
				if (node->meshData) {
					if (!node->meshData->bones.empty()) {
						hasBones = true;
					}
					// If we haven't processed this material ID yet
					if (materialIdMap.find(node->materialId) == materialIdMap.end()) {
						// Get material from world
						Material* material = world->getMaterial(node->materialId);
						if (material) {
							// Add to model materials and map the ID
							id_t newMatId = addMaterial(material);
							materialIdMap[node->materialId] = newMatId;
							cout << "Mapped world material id " << node->materialId << " with model material id " << newMatId << "\n";
						}
					}
				}

				// Process children
				for (auto* child : node->children) {
					processMaterials(child);
				}
			}
		};

		// Process all materials in the hierarchy
		processMaterials(importResult.rootNode);
		cout << "Mapped " << materialIdMap.size() << " materials!\n";

		// Convert nodes with updated material indices
		function<ModelNode* (MeshNodeData*, ModelNode*)> convertNode =
			[&](MeshNodeData* importNode, ModelNode* parent) -> ModelNode* {
			if (!importNode) return nullptr;

			//Map the Assimp node data to Model node data
			ModelNode* node = new ModelNode();
			node->nodeName = importNode->nodeName;
			node->meshData = importNode->meshData;
			node->parent = parent;
			node->localTransform = importNode->transformation;

			// Update bone data if this node has a mesh with bones
			if (node->meshData && !node->meshData->bones.empty()) {
				updateBoneData(node->meshData);
			}

			// Map the material ID
			if (materialIdMap.find(importNode->materialId) != materialIdMap.end()) {
				node->materialIndex = materialIdMap[importNode->materialId];
			} else {
				node->materialIndex = 0;  // Default to first material
			}

			// Convert children
			for (auto* childImport : importNode->children) {
				ModelNode* childNode = convertNode(childImport, node);
				if (childNode) {
					node->children.push_back(childNode);
				}
			}

			return node;
		};

		// Convert ImportResult to ModelNode hierarchy with proper material indices
		rootNode = convertNode(importResult.rootNode, nullptr);

		// Create animator if skeletal
		if (hasBones || !modelBones.empty()) {
			setupAnimations(sourcePath);
		}
		cout << "Successfully imported model from '" << sourcePath << "'!\n";
	}

	// Method for manually creating the Model from MeshData
	Model::Model(MeshData* meshData, string name) : sourcePath(name) {
		rootNode = createNode(name, meshData);
		updateBoneData(meshData);
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

		// Clean up materials
		for (auto* material : modelMaterials) {
			delete material;
		}

		// Clean up animations
		for (auto& [name, anim] : modelAnimations) {
			delete anim;
		}
	}
	
	id_t Model::instantiate(Transformation3D rootTransform, vector<Material*> overrideMaterials) {
		cout << "\n=== Instantiating Model: "<< rootNode->nodeName <<" ===\n";

		if (!rootNode) {
			cout << "Cannot instantiate - model has no root node\n";
			return 0;
		}

		//Use override materials if provided, otherwise use model materials
		const vector<Material*>& materialsToUse = overrideMaterials.empty() ? modelMaterials : overrideMaterials;

		// Ensure we have at least one material
		if (materialsToUse.empty()) {
			cout << "No materials available for model instantiation\n";
			modelMaterials.push_back(renderer.getFallbackMaterial());
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
		cout << "  - Total Body Count: " << bodyCount << "\n";
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
			return modelMaterials[index];
		}
		return nullptr;
	}
	id_t Model::addMaterial(Material* material) {
		modelMaterials.push_back(material);
		return modelMaterials.size() - 1;
	}

	void Model::setNodeMaterial(ModelNode* node, id_t materialIndex) {
		if (node && materialIndex < modelMaterials.size()) {
			node->materialIndex = materialIndex;
		}
	}
	void Model::setMaterial(id_t index, Material* material) {
		if (index >= modelMaterials.size()) {
			modelMaterials.resize(index + 1, nullptr);
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
		Animator* animator = new Animator(modelAnimations.begin()->second);
		return animator;
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
	void Model::createChildBodies(ModelNode* node, Body* parentBody, const vector<Material*>& materials) {
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

		// Create a mesh for this node (even if empty)
		Mesh* mesh = new Mesh(node->meshData, nodeTransform, node->meshData ? vector<Material*>{materials[node->materialIndex]} : vector<Material*>());
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
	bool Model::setupAnimations(const string& path) {
		cout << "\n=== Setting up Animations ===\n"
			<< "Source Path: " << path << "\n"
			<< "Bone count: " << modelBones.size() << "\n";

		//TODO: Create a class or higher-level function for this, as findSkeletalMesh is reused in MeshImporter.importAnimation
		// Find the mesh with bone data
		MeshData* skeletalMesh = nullptr;
		std::function<void(ModelNode*)> findSkeletalMesh =
			[&skeletalMesh, &findSkeletalMesh](ModelNode* node) {
			if (node->meshData && !node->meshData->bones.empty()) {
				skeletalMesh = node->meshData;
				return;
			}
			for (auto* child : node->children) {
				findSkeletalMesh(child);
				if (skeletalMesh) return;
			}
		};
		findSkeletalMesh(rootNode);
		if (!skeletalMesh) {
			cout << "WARNING: No skeletal mesh found for animation import\n";
			return false;
		}
		cout << "Found skeletal mesh with " << skeletalMesh->bones.size() << " bones\n";

		// Import animations
		if (!renderer.importAnimation(path, this)) {
			cout << "Failed to import animations from " << path << "\n";
			return false;
		}
		cout << "Successfully imported " << modelAnimations.size() << " animations!\n";

		// Create single animator instance for the model
		modelAnimator = createAnimator();
		if (!modelAnimator) {
			cout << "ERROR: Failed to create animator for " << path << "\n";
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
}