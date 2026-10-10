#include "Animator.h"
#include <cmath>
#include "../Engine/Engine.h"

namespace CGEngine {
	Animator::Animator(const string& animationName):currentAnimation(nullptr) {
		init();
		pose.reserve(100);
		for (int i = 0; i < 100; i++) {
			pose.push_back(glm::mat4(1.0f));
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

		float ticksPerSecond = currentAnimation->getTicksPerSecond();
		if (ticksPerSecond <= 0) {
			ticksPerSecond = 24.0f;
		}
		if (!paused) {
			currentTime += dt * speed * ticksPerSecond;
		}

		//Wrap time for looping, or hold the last pose
		float duration = currentAnimation->getDuration();
		if (duration > 0 && currentTime >= duration) {
			if (looping) {
				currentTime = fmod(currentTime, duration);
			} else {
				currentTime = duration;
			}
		}

		calculateBoneTransform(&currentAnimation->getRoot(), glm::mat4(1.0f));
	}

	string Animator::getCurrentAnimationName() const {
		return currentAnimation ? currentAnimation->getName() : string();
	}

	float Animator::getTimeSeconds() const {
		if (!currentAnimation) return 0.0f;
		float ticksPerSecond = currentAnimation->getTicksPerSecond() > 0 ? currentAnimation->getTicksPerSecond() : 24.0f;
		return currentTime / ticksPerSecond;
	}

	float Animator::getDurationSeconds() const {
		if (!currentAnimation) return 0.0f;
		float ticksPerSecond = currentAnimation->getTicksPerSecond() > 0 ? currentAnimation->getTicksPerSecond() : 24.0f;
		return currentAnimation->getDuration() / ticksPerSecond;
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

	//Recursively calculate the bone transformations for the current animation
	void Animator::calculateBoneTransform(const NodeData* node, glm::mat4 parentTransform) {
		string nodeName = node->name;
		glm::mat4 nodeTransform = node->transformation;
		
		//Iterate through Animation.bones and find the bone with the same name as the Animation NodeData
		//Bones are updated per frame with interpolated transformations
		Bone* bone = currentAnimation->findBone(nodeName);
		if (bone) {
			bone->update(currentTime);
			nodeTransform = bone->getLocalTransform();
		}
		glm::mat4 globalTransform = parentTransform * nodeTransform;

		//Apply bone offset from skeleton to pose bone transform
		if (skeleton && skeleton->hasBone(nodeName)) {
			optional<BoneData> boneData = skeleton->getBoneData(nodeName);
			if (boneData.has_value()) {
				pose[boneData.value().id] = globalTransform * boneData.value().offset;
			}
		}

		for (int i = 0; i < node->childrenCount; i++) {
			calculateBoneTransform(&node->children[i], globalTransform);
		}
	}

	vector<glm::mat4> Animator::getBoneMatrices() {
		return pose;
	}
}