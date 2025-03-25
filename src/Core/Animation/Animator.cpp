#include "Animator.h"

namespace CGEngine {
	Animator::Animator(Animation* animation) {
		deltaTime = 0.0;
		currentTime = 0.0;
		currentAnimation = animation;
		boneMatrices.reserve(100);
		for (int i = 0; i < 100; i++) {
			boneMatrices.push_back(glm::mat4(1.0f));
		}
	}

	void Animator::updateAnimation(float dt) {
		deltaTime = dt;
		if (currentAnimation) {
			currentTime += currentAnimation->getTicksPerSecond() * dt;
			currentTime = fmod(currentTime, currentAnimation->getDuration());
			calculateBoneTransform(&currentAnimation->getRoot(), glm::mat4(1.0f));
		}
	}

	void Animator::playAnimation(Animation* animation) {
		currentAnimation = animation;
		currentTime = 0.0f;
	}

	void Animator::calculateBoneTransform(const NodeData* node, glm::mat4 parentTransform) {
		string nodeName = node->name;
		glm::mat4 nodeTransform = node->transformation;
		Bone* bone = currentAnimation->findBone(nodeName);
		if (bone) {
			bone->update(currentTime);
			nodeTransform = bone->getLocalTransform();
		}
		glm::mat4 globalTransformation = parentTransform * nodeTransform;

		map<string,BoneData> boneData = currentAnimation->getBoneData();
		auto iter = boneData.find(nodeName);
		if (iter != boneData.end()) {
			if (nodeName != "Armature") {
				int index = iter->second.id;
				if (index >= 0 && index < 100) {
					glm::mat4 offset = boneData[nodeName].offset;
					boneMatrices[index] = globalTransformation * offset;
					//glm::quat rotation;
					//glm::decompose(boneMatrices[index], glm::vec3(), rotation, glm::vec3(), glm::vec3(), glm::vec4());
					//rotation = glm::conjugate(rotation);
					//cout << "Updating bone " << index << " " << nodeName << ": " << rotation.x << "," << rotation.y << "," << rotation.z << "\n";
				}
			}
		}
		for (int i = 0; i < node->childrenCount; i++) {
			//cout << "Updating children " << i << "\n";
			calculateBoneTransform(&node->children[i], globalTransformation);
		}
	}

	vector<glm::mat4> Animator::getBoneMatrices() {
		return boneMatrices;
	}
}