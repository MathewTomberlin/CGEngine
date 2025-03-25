#pragma once

#include "../World/Renderer.h"
#include "Animation.h"

namespace CGEngine {
	class Animator {
	public:
		Animator(Animation* animation);
		void updateAnimation(float dt);
		void playAnimation(Animation* animation);
		void calculateBoneTransform(const NodeData* node, glm::mat4 parentTransform);
		vector<glm::mat4> getBoneMatrices();

	private:
		vector<glm::mat4> boneMatrices;
		Animation* currentAnimation;
		float currentTime = 0.0;
		float deltaTime = 0.0;
	};
}