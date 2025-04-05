#pragma once

#include "../World/Renderer.h"
#include "../Mesh/Bone.h"

namespace CGEngine {
	class Bone;
	class Animation : public EngineSystem, public IResource {
	public:
		Animation();
		~Animation();
		Bone* findBone(const string name);
		inline float getTicksPerSecond() { return ticksPerSecond; }
		inline float getDuration() { return duration; }
		inline const NodeData& getRoot() { return root; }
		/// <summary>
		/// Recursively iterate through the scene heirarchy and apply scene node data to
		/// animation nodes. This is used to build the animation heirarchy from the scene heirarchy.
		/// </summary>
		/// <param name="toAnimationNode">The Animation node to be built</param>
		/// <param name="fromSceneNode">The scene node to parse</param>
		void importAnimationHeirarchy(NodeData& toAnimationNode, const aiNode* fromSceneNode);
		void importAnimationBones(const aiAnimation* animation, Skeleton* skeleton);
		string getName() const { return animationName; }
		void setName(const string& name) { animationName = name; }
		// Make root node accessible for hierarchy building
		const NodeData& getRoot() const { return root; }
		bool isValid() const;
	
		string animationName;
		float duration;
		int ticksPerSecond;
		/// <summary>
		/// Animation keyframed bones, updated per frame by the Animator by combining Bone transforms with parent transforms
		/// </summary>
		vector<Bone> bones;
		/// <summary>
		/// Animation node heirarchy is used to navigate heirarchy when calculating transforms in Animator
		/// </summary>
		NodeData root;
	};
}