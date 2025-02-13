#pragma once

#include "Behavior.h"
#include "../World/World.h"
#include "../Scripts/Script.h"
using namespace std;

namespace CGEngine {
	enum AnimationState { Ready, Running, Paused };

	class AnimationBehavior : public Behavior {
	public:
		AnimationBehavior(Body* owner);
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