#include "Animation.h"
#include "../Engine/Engine.h"

using namespace std;
namespace CGEngine {
	Animation::Animation(const string& path, MeshData* mesh) {
		unsigned int flags = aiProcess_Triangulate |
			aiProcess_GenNormals |
			aiProcess_LimitBoneWeights |
			aiProcess_ConvertToLeftHanded |
			aiProcess_ValidateDataStructure |
			aiProcess_JoinIdenticalVertices |
			aiProcess_OptimizeGraph |
			aiProcess_OptimizeMeshes;
		const aiScene* scene = renderer.readFile(path, flags);
		aiAnimation* animation = scene->mAnimations[0];
		duration = animation->mDuration;
		ticksPerSecond = animation->mTicksPerSecond;
		cout << "Imported animation " << animation->mName.C_Str() << " with duration " << duration << " frames and ticksPerSecond " << ticksPerSecond << "\n";
		readHeirarchyData(root, scene->mRootNode);
		readMissingBones(animation, mesh);
	}

	Bone* Animation::findBone(const string name) {
		auto iter = std::find_if(bones.begin(), bones.end(), [&](const Bone& bone) { return bone.getBoneName() == name; });
		if (iter == bones.end()) return nullptr;
		else return &(*iter);
	}

	void Animation::readHeirarchyData(NodeData& dest, const aiNode* src) {
		dest.name = src->mName.C_Str();
		dest.transformation = MeshImporter::fromAiMatrix4toGlm(src->mTransformation);
		dest.childrenCount = src->mNumChildren;

		for (int i = 0; i < src->mNumChildren; i++) {
			NodeData newData;
			readHeirarchyData(newData, src->mChildren[i]);
			dest.children.push_back(newData);
			cout << "Adding node " << newData.name << " as child of " << dest.name << "\n";
		}
	}

	void Animation::readMissingBones(const aiAnimation* animation, MeshData* mesh) {
		//Get Bone Ids/Offsets and Bone Count from Mesh
		auto& boneInfoMap = mesh->bones;
		int& boneCount = mesh->boneCounter;
		
		//Read animation channels
		int channels = animation->mNumChannels;
		for (int i = 0; i < channels; i++) {
			auto channel = animation->mChannels[i];
			//Get the bone name for the animation channel
			std::string boneName = channel->mNodeName.data;
			//If the Mesh's Bones doesn't include the boneName, add it with boneCount id and increment boneCount
			if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
				boneInfoMap[boneName].id = boneCount;
				boneCount++;
				cout << "Read bone " << boneInfoMap[boneName].id << ": " << boneName << "\n";
			}
			bones.push_back(Bone(channel->mNodeName.data, boneInfoMap[channel->mNodeName.data].id, channel));
		}

		//Update the mesh's bones and boneCounter
		mesh->bones = boneInfoMap;
		mesh->boneCounter = boneCount;

		//Update the animation's bone data
		boneData = boneInfoMap;
		cout << "Read " << channels << " animation channels with " << boneCount << " bones " << "\n";
	}
}