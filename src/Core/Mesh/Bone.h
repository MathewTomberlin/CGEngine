#pragma once

#include "../World/Renderer.h"

using namespace std;

namespace CGEngine {
	class Bone {
	private:
		vector<KeyPosition> positions;
		vector<KeyRotation> rotations;
		vector<KeyScale> scales;
		int numPositions;
		int numRotations;
		int numScales;

		glm::mat4 localTransform;
		string name;
		int id;
	public:
		Bone(const string& name, int id, const aiNodeAnim* channel);
		void update(float animTime);
		glm::mat4 getLocalTransform();
		string getBoneName() const;
		int getBoneId();
		int getPositionIndex(float animTime);
		int getRotationIndex(float animTime);
		int getScaleIndex(float animTime);
	private:
		float getScaleFactor(float lastTimestamp, float nextTimestamp, float animTime);
		glm::mat4 interpolatePosition(float animTime);
		glm::mat4 interpolateRotation(float animTime);
		glm::mat4 interpolateScale(float animTime);
	};
}