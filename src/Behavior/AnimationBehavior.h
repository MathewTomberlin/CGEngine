#pragma once

#include "Behavior.h"
#include "../World/World.h"
#include "../Scripts/Script.h"
using namespace std;

namespace CGEngine {
	enum AnimationState { Ready, Running, Paused };

	struct AnimationParameters {
		AnimationParameters(float rate = 15.f, float spd = 1.f, int maxFrm = 7, bool loop = true, bool startRun = true) :frameRate(rate), speed(spd), maxFrame(maxFrm), looping(loop), startRunning(startRun) {
		
		}

		float frameRate = 15.0f;
		float speed = 1.0f;
		int maxFrame = 7;
		bool looping = true;
		bool startRunning = false;
	};

	class AnimationBehavior : public Behavior {
	public:
		AnimationBehavior(Body* owner, AnimationParameters params = AnimationParameters());
		void setParameters(AnimationParameters params);
		void resetProcessData();
	private:
		static ScriptEvent animBehaviorStartEvt;
		static ScriptEvent calculateFrameLengthEvt;
		static ScriptEvent pauseAnimEvt;
		static ScriptEvent endAnimEvt;
		static ScriptEvent startAnimEvt;
		static ScriptEvent animateEvt;
		static ScriptEvent animBehaviorUpdateEvt;
		static ScriptEvent onTranslateEvt;
	};
}