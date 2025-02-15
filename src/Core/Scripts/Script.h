#pragma once

#include <functional>
#include <optional>
#include "../Types/Types.h"
#include "../Types/DataMap.h"
#include "../Types/DataControllers/InputDataController.h"
#include "../Types/DataControllers/OutputDataController.h"
using namespace std;

namespace CGEngine {
	class Body;
	class Script;
	class Behavior;

	struct ScArgs {
	public:
		ScArgs(Script* s, Body* b = nullptr, Behavior* beh = nullptr) {
			script = s;
			caller = b;
			behavior = beh;
		}
		Script* script;
		Body* caller = nullptr;
		Behavior* behavior = nullptr;
	};

	typedef function<void(ScArgs)> ScriptEvent;

	class Script : public InputDataController, public OutputDataController {
	public:
		Script(ScriptEvent evt);

		ScriptEvent scriptEvent;
		optional<size_t> id;

		virtual void call(Body* caller = nullptr, Behavior* behavior = nullptr) {
			scriptEvent(ScArgs(this, caller, behavior));
		}
	};
}