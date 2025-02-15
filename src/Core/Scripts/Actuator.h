#pragma once

#include "Script.h"

namespace CGEngine {
	class Actuator : public Script {
	public:
		Actuator(ScriptEvent s, Body* calling = nullptr, Behavior* behavior = nullptr) : caller(calling), behavior(behavior), Script(s) { }

		void call(Body* caller = nullptr, Behavior* behavior = nullptr) override {
			scriptEvent(ScArgs(this, this->caller, this->behavior)); 
		}
	protected:
		Body* caller = nullptr;
		Behavior* behavior = nullptr;
	};
}