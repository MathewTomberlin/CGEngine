#include "Bone.h"

namespace CGEngine {
	Bone::Bone(const string& name, int id, const aiNodeAnim* channel) : name(name), id(id), localTransform(1.0f) {
		numPositions = channel->mNumPositionKeys;
		for (int positionIndex = 0; positionIndex < numPositions; ++positionIndex) {
			aiVector3D aiPosition = channel->mPositionKeys[positionIndex].mValue;
			float timestamp = channel->mPositionKeys[positionIndex].mTime;
			KeyPosition data;
			data.position = MeshImporter::aiV3toGlm(aiPosition);
			data.timeStamp = timestamp;
			positions.push_back(data);
		}
		numRotations = channel->mNumRotationKeys;
		for (int rotationIndex = 0; rotationIndex < numRotations; ++rotationIndex) {
			aiQuaternion aiOrientation = channel->mRotationKeys[rotationIndex].mValue;
			float timestamp = channel->mRotationKeys[rotationIndex].mTime;
			KeyRotation data;
			data.orientation = MeshImporter::fromAiQuatToGlm(aiOrientation);
			data.timeStamp = timestamp;
			rotations.push_back(data);
		}
		numScales = channel->mNumScalingKeys;
		for (int scaleIndex = 0; scaleIndex < numScales; ++scaleIndex) {
			aiVector3D aiScale = channel->mScalingKeys[scaleIndex].mValue;
			float timestamp = channel->mScalingKeys[scaleIndex].mTime;
			KeyScale data;
			data.scale = MeshImporter::aiV3toGlm(aiScale);
			data.timeStamp = timestamp;
			scales.push_back(data);
		}
	}

	void Bone::update(float animTime) {
		glm::mat4 translation = interpolatePosition(animTime);
		glm::mat4 rotation = interpolateRotation(animTime);
		glm::mat4 scale = interpolateScale(animTime);
		localTransform = translation * rotation * scale;
	}

	glm::mat4 Bone::getLocalTransform() { return localTransform; }
	string Bone::getBoneName() const { return name; }
	int Bone::getBoneId() { return id; }

	int Bone::getPositionIndex(float animTime) {
		for (int index = 0; index < numPositions - 1; ++index) {
			if (index + 1 >= positions.size()) {
				cout << "Invalid position index" << "\n";
				return index;
			}
			if (animTime < positions[index + 1].timeStamp) {
				return index;
			}
		}
		return numPositions - 1;
	}

	int Bone::getRotationIndex(float animTime) {
		for (int index = 0; index < numRotations - 1; ++index) {
			if (index + 1 >= rotations.size()) {
				cout << "Invalid rotation index" << "\n";
				return index;
			}
			if (animTime < rotations[index + 1].timeStamp) {
				return index;
			}
		}
		return numRotations - 1;
	}

	int Bone::getScaleIndex(float animTime) {
		for (int index = 0; index < numScales - 1; ++index) {
			if (index + 1 >= scales.size()) {
				cout << "Invalid scale index" << "\n";
				return index;
			}
			if (animTime < scales[index + 1].timeStamp) {
				return index;
			}
		}
		return numScales - 1;
	}
	float Bone::getScaleFactor(float lastTimestamp, float nextTimestamp, float animTime) {
		float scaleFactor = 0.0f;
		float midwayLenth = animTime - lastTimestamp;
		float framesDiff = nextTimestamp - lastTimestamp;
		scaleFactor = midwayLenth / framesDiff;
		return scaleFactor;
	}

	glm::mat4 Bone::interpolatePosition(float animTime) {
		if (numPositions == 1) {
			return glm::translate(glm::mat4(1.0f), positions[0].position);
		}

		int index0 = getPositionIndex(animTime);
		int index1 = index0 + 1;
		glm::vec3 finalPosition = positions[index0].position;
		if (index1 < positions.size()) {
			float scaleFactor = getScaleFactor(positions[index0].timeStamp, positions[index1].timeStamp, animTime);
			finalPosition = glm::mix(positions[index0].position, positions[index1].position, scaleFactor);
			//cout << "Interpolated Pos: " << finalPosition.x << "," << finalPosition.y << "," << finalPosition.z << "\n";
		}
		return glm::translate(glm::mat4(1.0f), finalPosition);
	}

	glm::mat4 Bone::interpolateRotation(float animTime) {
		if (numRotations == 1) {
			auto rotation = glm::normalize(rotations[0].orientation);
			return glm::mat4(rotation);
		}

		int index0 = getRotationIndex(animTime);
		int index1 = index0 + 1;
		glm::quat finalRotation = rotations[index0].orientation;
		if (index1 < rotations.size()) {
			float scaleFactor = getScaleFactor(rotations[index0].timeStamp, rotations[index1].timeStamp, animTime);
			finalRotation = glm::slerp(rotations[index0].orientation, rotations[index1].orientation, scaleFactor);
			finalRotation = glm::normalize(finalRotation);
			//cout << "Interpolated Rot: " << finalRotation.x << "," << finalRotation.y << "," << finalRotation.z << "," << finalRotation.w <<"\n";
		}
		return glm::mat4(finalRotation);
	}

	glm::mat4 Bone::interpolateScale(float animTime) {
		if (numScales == 1) {
			return glm::scale(glm::mat4(1.0f), scales[0].scale);
		}

		int index0 = getScaleIndex(animTime);
		int index1 = index0 + 1;
		glm::vec3 finalScale = scales[index0].scale;
		if (index1 < scales.size()) {
			float scaleFactor = getScaleFactor(scales[index0].timeStamp, scales[index1].timeStamp, animTime);
			finalScale = glm::mix(scales[index0].scale, scales[index1].scale, scaleFactor);
		}
		return glm::scale(glm::mat4(1.0f), finalScale);
	}
}