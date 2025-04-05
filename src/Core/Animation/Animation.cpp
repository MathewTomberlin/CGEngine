#include "Animation.h"
#include "../Engine/Engine.h"

using namespace std;
namespace CGEngine {
	Animation::Animation() :duration(0), ticksPerSecond(24.0f) { 
		init();
		setLogLevel(LogInfo);
	}

	Animation::~Animation() { }

	Bone* Animation::findBone(const string name) {
		auto iter = find_if(bones.begin(), bones.end(), [&](const Bone& bone) { return bone.getBoneName() == name; });
		if (iter == bones.end()) return nullptr;
		else return &(*iter);
	}

	void Animation::importAnimationHeirarchy(NodeData& toAnimationNode, const aiNode* fromSceneNode) {
		if (!fromSceneNode) {
			log(this, LogError, "Invalid source node");
			return;
		}

		try {
			//Convert scene node to animation node
			toAnimationNode.name = fromSceneNode->mName.C_Str();
			toAnimationNode.transformation = MeshImporter::fromAiMatrix4toGlm(fromSceneNode->mTransformation);
			toAnimationNode.childrenCount = fromSceneNode->mNumChildren;
			//For each scene node child, convert it and add the new node to the animation node children
			for (int i = 0; i < fromSceneNode->mNumChildren; i++) {
				NodeData newData;
				importAnimationHeirarchy(newData, fromSceneNode->mChildren[i]);
				toAnimationNode.children.push_back(newData);
			}
		} catch (const std::exception& e) {
			string nodeName = fromSceneNode->mName.length ? fromSceneNode->mName.C_Str() : "unnamed";
			log(this, LogError, "Error importing animation node {}: {}", nodeName, e.what());
		}
	}

	void Animation::importAnimationBones(const aiAnimation* animation, Skeleton* skeleton) {
		if (!animation) {
			log(this, LogError, "Invalid animation data");
			return;
		}
		if (!skeleton) {
			log(this, LogError, "Invalid skeleton");
			return;
		}
		
		//Read animation channels
		for (unsigned int i = 0; i < animation->mNumChannels; i++) {
			auto channel = animation->mChannels[i];
			if (!channel) continue;

			//Get the bone name for the animation channel
			string boneName = channel->mNodeName.data;
			//Create or update the Bone with the name from the channel and
			//the id for that name in the skeleton
			optional<BoneData>& boneData = skeleton->getBoneData(boneName);
			if (boneData.has_value()) {
				bones.push_back(Bone(boneName, boneData.value().id, channel));
			}
		}
	}

	bool Animation::isValid() const {
		return true;
	}
}