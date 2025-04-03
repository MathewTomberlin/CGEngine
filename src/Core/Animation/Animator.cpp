#include "Animator.h"
#include "../Engine/Engine.h"

namespace CGEngine {
	Animator::Animator(const string& animationName) {
		init();
		boneMatrices.reserve(100);
		for (int i = 0; i < 100; i++) {
			boneMatrices.push_back(glm::mat4(1.0f));
		}
		optional<id_t> animationId = assets.getId<Animation>(animationName);
		if (animationId.has_value()) {
			currentAnimation = assets.get<Animation>(animationId.value());
			currentTime = 0.0f;
		}
		else {
			log(this, LogError, "Animation '{}' not found in AssetManager", animationName);
		}
	}

	void Animator::updateAnimation(float dt) {
		if (!currentAnimation) {
			log(this, LogError, "No animation assigned to animator");
			return;
		}

		// Use a fixed ticks per second if none specified
		float ticksPerSecond = currentAnimation->getTicksPerSecond();
		if (ticksPerSecond <= 0) {
			ticksPerSecond = 24.0f; // Standard animation rate
		}
		currentTime += dt * ticksPerSecond;
		// Wrap time for looping
		float duration = currentAnimation->getDuration();
		if (currentTime >= duration) {
			currentTime = 0;
		}
		calculateBoneTransform(&currentAnimation->getRoot(), glm::mat4(1.0f));
	}

	void Animator::playAnimation(const string& animationName) {
		optional<id_t> animationId = assets.getId<Animation>(animationName);
		if (animationId.has_value()) {
			currentAnimation = assets.get<Animation>(animationId.value());
			currentTime = 0.0f;
		}
		else {
			log(this, LogError, "Animation '{}' not found in AssetManager", animationName);
		}
	}

	void Animator::calculateBoneTransform(const NodeData* node, glm::mat4 parentTransform) {
		string nodeName = node->name;
		glm::mat4 nodeTransform = node->transformation;
		Bone* bone = currentAnimation->findBone(nodeName);
		if (bone) {
			bone->update(currentTime);
			nodeTransform = bone->getLocalTransform();
		}
		glm::mat4 globalTransform = parentTransform * nodeTransform;

		auto& boneInfoMap = currentAnimation->getBoneData();
		if (boneInfoMap.find(nodeName) != boneInfoMap.end()) {
			int index = boneInfoMap.at(nodeName).id;
			glm::mat4 offset = boneInfoMap.at(nodeName).offset;
			boneMatrices[index] = globalTransform * offset;
		}
		for (int i = 0; i < node->childrenCount; i++) {
			calculateBoneTransform(&node->children[i], globalTransform);
		}
	}

	vector<glm::mat4> Animator::getBoneMatrices() {
		return boneMatrices;
	}
}