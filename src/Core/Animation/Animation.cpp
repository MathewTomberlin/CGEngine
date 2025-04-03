#include "Animation.h"
#include "../Engine/Engine.h"

using namespace std;
namespace CGEngine {
	Animation::Animation() :duration(0), ticksPerSecond(24.0f) { 
		init();
		setLogLevel(LogInfo);
	}

	Animation::~Animation() {
		
	}

	Bone* Animation::findBone(const string name) {
		auto iter = std::find_if(bones.begin(), bones.end(), [&](const Bone& bone) { return bone.getBoneName() == name; });
		if (iter == bones.end()) return nullptr;
		else return &(*iter);
	}

	void Animation::readHeirarchyData(NodeData& dest, const aiNode* src) {
		if (!src) {
			log(this, LogError, "Invalid source node");
			return;
		}

		try {
			dest.name = src->mName.C_Str();
			dest.transformation = MeshImporter::fromAiMatrix4toGlm(src->mTransformation);
			dest.childrenCount = src->mNumChildren;
			for (int i = 0; i < src->mNumChildren; i++) {
				NodeData newData;
				readHeirarchyData(newData, src->mChildren[i]);
				dest.children.push_back(newData);
			}
		} catch (const std::exception& e) {
			string nodeName = src->mName.length ? src->mName.C_Str() : "unnamed";
			log(this, LogError, "Error processing node {}: {}", nodeName, e.what());
		}
	}

	void Animation::readMissingBones(const aiAnimation* animation, map<string,BoneData> modelBones) {
		if (!animation) {
			log(this, LogError, "Invalid animation data");
			return;
		}
		if (modelBones.empty()) {
			log(this, LogError, "Invalid mesh data");
			return;
		}

		//Get Bone Ids/Offsets and Bone Count from Mesh
		auto& boneInfoMap = modelBones;
		
		//Read animation channels
		for (unsigned int i = 0; i < animation->mNumChannels; i++) {
			auto channel = animation->mChannels[i];
			if (!channel) continue;

			//Get the bone name for the animation channel
			string boneName = channel->mNodeName.data;
			// Create or update bone data
			BoneData& boneData = boneInfoMap[boneName];
			bones.push_back(Bone(boneName, boneData.id, channel));
		}

		//Update the animation's bone data
		boneData = boneInfoMap;
	}

	bool Animation::isValid() const {
		return true;
	}
}