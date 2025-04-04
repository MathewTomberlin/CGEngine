#pragma once

#include "../World/Renderer.h"
#include "../Skeleton/Skeleton.h"

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

	class Model : public EngineSystem, public IResource {
	public:
		//Constructor to create a Model from an imported file via Assimp
		Model(string sourcePath, const string& skeletonName = "");
		//Constructor to create a Model manually, likely from MeshData
		Model(MeshData* meshData, string name = "", string skeletonName = "");
		~Model();
		//Converts imported MeshNodeData to ModelNode
		ModelNode* meshNodeToModelNode(MeshNodeData* meshNode, ModelNode* modelNode);
		//Create a hierarchy of Bodies from imported MeshNodeData and return the root Body id
		optional<id_t> instantiate(Transformation3D rootTransform = Transformation3D(), vector<id_t> overrideMaterials = {});
		//Return whether the model has bones
		bool isSkeletal() const;
		//Return the model import path
		string getModelPath() const { return sourcePath; };
		//Return the model animator
		Animator* getAnimator() const { return modelAnimator; }
		//Create and return an animator if the model is skeletal
		Animator* createAnimator() const;
		// Add methods for manually building hierarchy
		ModelNode* createNode(string name, MeshData* meshData = nullptr, id_t materialIndex = 0);
		//Return the model root node
		ModelNode* getRootNode() const { return rootNode; }
		//Return a vector of the model materials
		vector<Material*> getMaterials();
		bool isValid() const; //TODO: Properly implement isValid in Model
	private:
		friend class MeshImporter;

		// Model data
		string sourcePath;
		ModelNode* rootNode = nullptr;
		Skeleton* modelSkeleton = nullptr;
		map<string, BoneData> modelBones;
		vector<id_t> modelMaterials;
		vector<string> modelAnimations;
		map<string, AnimationNodeMapping> animationNodeMap;
		Animator* modelAnimator = nullptr;
		size_t bodyCount = 0;

		// Helper to update modelBones when adding mesh data
		void updateBoneData(const MeshData* meshData);
		void setSkeleton(Skeleton* skeleton);
		//During instantiation, recursively create Bodies for each child node
		void createChildBodies(ModelNode* node, Body* parentBody, const vector<id_t>& materials);
		//Delete unused pointers
		void cleanupModelNodes(ModelNode* node);

		// Prevent copying
		Model(const Model&) = delete;
		Model& operator=(const Model&) = delete;
	};
}