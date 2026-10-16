#pragma once

#include <map>

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
		/// The model-space transform of a node of the clip at the current time, with the bone's inverse bind pose not applied: where the
		/// bone is, for an object that follows it. False if the clip has no node of that name.
		bool getNodeTransform(const string& nodeName, glm::mat4& out) const;
		void setSkeleton(Skeleton* skeleton) { this->skeleton = skeleton; }
		/// Name of the clip being played, or empty if none.
		string getCurrentAnimationName() const;
		/// Playback speed multiplier (1 = normal, 0.5 = half speed). Negative values are not supported.
		void setSpeed(float speed) { this->speed = speed; }
		float getSpeed() const { return speed; }
		/// Looping clips wrap at the end; non-looping clips hold their last pose.
		void setLooping(bool looping) { this->looping = looping; }
		bool isLooping() const { return looping; }
		void setPaused(bool paused) { this->paused = paused; }
		bool isPaused() const { return paused; }
		/// Current position in the clip, in seconds.
		float getTimeSeconds() const;
		/// Length of the current clip in seconds (0 if none).
		float getDurationSeconds() const;
	private:
		float speed = 1.0f;
		bool looping = true;
		bool paused = false;
		/// <summary>
		/// The matrix transformation passed to the shader for each bone in a pose
		/// </summary>
		vector<glm::mat4> pose;
		/// Each node's model-space transform from the last update (see getNodeTransform).
		map<string, glm::mat4> nodeTransforms;
		Animation* currentAnimation;
		float currentTime = 0.0;
		/// <summary>
		/// Oberservation pointer of the Skeleton owned by AssetManager
		/// </summary>
		Skeleton* skeleton = nullptr;
		/// Inverse of each bone's rest global transform, from the same hierarchy the animator walks.
		/// Used for skinning so the rest pose is exactly the bind pose, whatever the imported offsets say.
		std::map<std::string, glm::mat4> inverseRest;
		const Animation* restBuiltFor = nullptr;
		void buildRestPose();
		void collectRest(const NodeData& node, glm::mat4 parentTransform);
	};
}