#pragma once

#include "Script.h"

namespace CGEngine {
	class Actuator : public Script {
	public:
		Actuator(ScriptEvent s, Body* calling = nullptr, Behavior* behavior = nullptr) : caller(calling), behavior(behavior), Script(s) { }

		Body* caller = nullptr;
		Behavior* behavior = nullptr;
		void call(Body* caller = nullptr, Behavior* behavior = nullptr) override {
			scriptEvent(ScArgs(this, this->caller, this->behavior)); 
		}
	};
}