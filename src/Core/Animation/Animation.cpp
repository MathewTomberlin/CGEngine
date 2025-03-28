#include "Animation.h"
#include "../Engine/Engine.h"

using namespace std;
namespace CGEngine {
	Animation::Animation() :duration(0), ticksPerSecond(24.0f) { }

	Bone* Animation::findBone(const string name) {
		auto iter = std::find_if(bones.begin(), bones.end(), [&](const Bone& bone) { return bone.getBoneName() == name; });
		if (iter == bones.end()) return nullptr;
		else return &(*iter);
	}

	void Animation::readHeirarchyData(NodeData& dest, const aiNode* src) {
		if (!src) {
			cout << "Error: Invalid source node\n";
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
		}
		catch (const std::exception& e) {
			cout << "Error processing node " << (src->mName.length ? src->mName.C_Str() : "unnamed")
				<< ": " << e.what() << "\n";
		}
	}

	void Animation::readMissingBones(const aiAnimation* animation, map<string,BoneData> modelBones) {
		if (!animation) {
			cout << "Error: Invalid animation data\n";
			return;
		}
		if (modelBones.empty()) {
			cout << "Error: Invalid mesh data\n";
			return;
		}

		//Get Bone Ids/Offsets and Bone Count from Mesh
		auto& boneInfoMap = modelBones;

		cout << "Processing Animation: "<< animation->mName.C_Str()<<"\n"
			<< "  Channels: " << animation->mNumChannels << "\n"
			<< "  MeshData Bones: " << modelBones.size() << "\n";
		
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
}