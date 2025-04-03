#pragma once

#include "../World/Renderer.h"
#include "Animation.h"

namespace CGEngine {
	class Animator : public EngineSystem {
	public:
		Animator(const string& animationName);
		void updateAnimation(float dt);
		void playAnimation(const string& animationName);
		void calculateBoneTransform(const NodeData* node, glm::mat4 parentTransform);
		vector<glm::mat4> getBoneMatrices();
	private:
		vector<glm::mat4> boneMatrices;
		Animation* currentAnimation;
		float currentTime = 0.0;
	};
}