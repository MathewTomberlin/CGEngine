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

	/// <summary>
	/// A Script is a stateless or stateful callable object. A Script holds a ScriptEvent function pointer that
	/// is called when the Script's call method is invoked. The ScriptEvent is passed ScArgs, which contains the
	/// SciptEvent's calling Script (which can be used to access the Script's input and output DataMaps), the
	/// Script's calling Body, and/or the Script's calling Behavior. Scripts are typically contained within a ScriptDomain
	/// and identified by unique id within that domain. Scripts may be called individually by script id within their domain
	/// or as a group whenever the domain itself is called.
	/// </summary>
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