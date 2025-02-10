#pragma once

#include "Script.h"

namespace CGEngine {
	class Actuator : public Script {
	public:
		Actuator(ScriptEvent s, Body* calling = nullptr) : caller(calling), Script(s) { }

		Body* caller = nullptr;
		void call(Body* caller = nullptr) override {
			scriptEvent(ScArgs(this, this->caller)); 
		}
	};
}