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

	void Animation::readMissingBones(const aiAnimation* animation, MeshData* mesh) {
		if (!animation) {
			cout << "Error: Invalid animation data\n";
			return;
		}
		if (!mesh || mesh->bones.empty()) {
			cout << "Error: Invalid mesh data\n";
			return;
		}

		//Get Bone Ids/Offsets and Bone Count from Mesh
		auto& boneInfoMap = mesh->bones;
		int& boneCount = mesh->boneCounter;

		cout << "\nProcessing Animation: "<< animation->mName.C_Str()<<"\n"
			<< "  Channels: " << animation->mNumChannels << "\n"
			<< "  MeshData Bones: " << mesh->bones.size() << "\n";
		
		//Read animation channels
		for (unsigned int i = 0; i < animation->mNumChannels; i++) {
			auto channel = animation->mChannels[i];
			if (!channel) continue;

			//Get the bone name for the animation channel
			std::string boneName = channel->mNodeName.data;
			//Possibly obsolete debug output
			//cout << "  Channel " << i << ": " << channel->mNodeName.data
			//	<< " [pos: " << channel->mNumPositionKeys
			//	<< ", rot: " << channel->mNumRotationKeys
			//	<< ", scale: " << channel->mNumScalingKeys << "]\n";
			// Create or update bone data
			BoneData& boneData = boneInfoMap[boneName];
			if (boneData.id == 0) {
				boneData.id = boneCount++;
			}
			bones.push_back(Bone(boneName, boneData.id, channel));
		}

		//Update the animation's bone data
		boneData = boneInfoMap;
		cout << "  Found Bones: " << boneCount << "\n";
	}
}