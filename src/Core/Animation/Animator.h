#pragma once

#include "../World/Renderer.h"
#include "Animation.h"

namespace CGEngine {
	class Animator : public EngineSystem {
	public:
		Animator(const string& animationName);
		/// <summary>
		/// Called by Renderer each frame to update the animation time and calculate the bone transformations
		/// </summary>
		/// <param name="dt">Delta time this frame</param>
		void updateAnimation(float dt);
		void playAnimation(const string& animationName);
		void calculateBoneTransform(const NodeData* node, glm::mat4 parentTransform);
		vector<glm::mat4> getBoneMatrices();
		void setSkeleton(Skeleton* skeleton) { this->skeleton = skeleton; }
	private:
		/// <summary>
		/// The matrix transformation passed to the shader for each bone in a pose
		/// </summary>
		vector<glm::mat4> pose;
		Animation* currentAnimation;
		float currentTime = 0.0;
		/// <summary>
		/// Oberservation pointer of the Skeleton owned by AssetManager
		/// </summary>
		Skeleton* skeleton = nullptr;
	};
}