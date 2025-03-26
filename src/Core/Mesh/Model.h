#pragma once

#include "../World/Renderer.h"

using namespace std;

namespace CGEngine {
	// This class is a container for meshes associated with an entity. Its exact purpose is
	// to facilitate mesh importing. When a mesh is imported via Assimp, its daa is stored in
	// a tree structure with a RootNode and child nodes. This tree structure is imported and
	// will be stored in this Model class. When the Model is instantiated, each node is visited
	// and a Body is created for each node and attached to its parent Body. Each node with a mesh
	// will have a Mesh owned by the Body. This class may be required to contain the materials for
	// the model so that each mesh can index into the Model materials. Additionally, this class will
	// contain the Animator for the model, if it is skeletal. Any additional data shared between Bodies
	// in the model should be stored here.

	struct ModelNode {
		string nodeName;
		MeshData* meshData = nullptr;     // Null if node has no mesh
		id_t materialIndex = 0;           // Index into modelMaterials
		glm::mat4 localTransform;
		ModelNode* parent = nullptr;
		vector<ModelNode*> children;
	};

	struct AnimationNodeMapping {
		ModelNode* modelNode;
		NodeData* animationNode;
		glm::mat4 bindPose;
	};

	class Animation;

	class Model {
	public:
		//Constructor to create a Model from an imported file via Assimp
		Model(string sourcePath);
		//Constructor to create a Model manually, likely from MeshData
		Model(MeshData* meshData, string name = "");
		~Model();

		//Create a hierarchy of Bodies from imported MeshNodeData and return the root Body id
		id_t instantiate(Transformation3D rootTransform = Transformation3D(), vector<Material*> overrideMaterials = {});

		//Get Model info
		bool isSkeletal() const { return !modelBones.empty(); };
		string getModelPath() const { return sourcePath; };
		size_t getBoneCount() const { return modelBones.size(); };

		// Material management
		id_t getMaterialCount() const { return modelMaterials.size(); }
		Material* getMaterial(id_t index);
		void setMaterial(id_t index, Material* material);
		id_t addMaterial(Material* material);
		void setNodeMaterial(ModelNode* node, id_t materialIndex);

		// Animation
		Animation* getAnimation(string animationName);
		vector<string> getAnimationNames() const;
		Animator* createAnimator() const;
		void addAnimation(Animation* animation);

		// Add methods for manually building hierarchy
		ModelNode* createNode(string name, MeshData* meshData = nullptr, id_t materialIndex = 0);
		void attachNode(ModelNode* parent, ModelNode* child);

		// Helper method to convert NodeData to ModelNode for animations
		ModelNode* convertAnimationNode(const NodeData& animNode, ModelNode* parent = nullptr);
		void mapAnimationNodes(const Animation* anim);
		void validateHierarchy() const;
		// Add new method to handle animation import and setup
		bool setupAnimations(const string& path);
	private:
		friend class MeshImporter;

		// Model data
		string sourcePath;
		ModelNode* rootNode = nullptr;
		map<string, BoneData> modelBones;
		vector<Material*> modelMaterials;
		map<string, Animation*> modelAnimations;
		map<string, AnimationNodeMapping> animationNodeMap;

		// Helper to update modelBones when adding mesh data
		void updateBoneData(const MeshData* meshData);
		// Helper to deep copy a node hierarchy
		ModelNode* copyNodeHierarchy(const ModelNode* source, ModelNode* newParent = nullptr);
		void mapAnimationNodeRecursive(const NodeData& animNode, ModelNode* modelNode);
		void createChildBodies(ModelNode* node, Body* parentBody, const vector<Material*>& materials);
		void cleanupModelNodes(ModelNode* node);

		// Prevent copying
		Model(const Model&) = delete;
		Model& operator=(const Model&) = delete;
	};
}