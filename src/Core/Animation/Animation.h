#pragma once

#include "../World/Renderer.h"
#include "../Mesh/Bone.h"

namespace CGEngine {
	class Bone;
	class Animation {
	public:
		Animation() = default;
		Animation(const string& path, MeshData* mesh);
		Bone* findBone(const string name);
		inline float getTicksPerSecond() { return ticksPerSecond; }
		inline float getDuration() { return duration; }
		inline const NodeData& getRoot() { return root; }
		inline const std::map<std::string, BoneData>& getBoneData() { return boneData; }
		void readHeirarchyData(NodeData& dest, const aiNode* src);
		void readMissingBones(const aiAnimation* animation, MeshData* mesh);

		float duration;
		int ticksPerSecond;
		std::vector<Bone> bones;
		NodeData root;
		std::map<std::string, BoneData> boneData;
	};
}