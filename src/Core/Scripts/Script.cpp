#include "Script.h"
#include <iostream>

namespace CGEngine {
	Script::Script(ScriptEvent evt) {
		scriptEvent = evt;
	}

	Script::Script(pybind11::object callable) : pyScriptEvent(std::move(callable)), scriptEvent(nullptr) {
	
	}

	void Script::setPyScriptEvent(pybind11::object callable) {
		pyScriptEvent = std::move(callable);
		scriptEvent = nullptr;
	}
}