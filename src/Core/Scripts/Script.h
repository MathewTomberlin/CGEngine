#pragma once

#include <functional>
#include <pybind11/pybind11.h>
#include <iostream>
#include "../Types/Types.h"
#include "../Types/DataControllers/InputDataController.h"
#include "../Types/DataControllers/OutputDataController.h"
using std::function;

namespace CGEngine {
	class Body;
	class Script;
	class Behavior;
	class DataMap;

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
		using ScriptEventHandler = std::variant<ScriptEvent, pybind11::object>;

		Script(ScriptEventHandler handler);

		virtual ~Script() = default;

		optional<size_t> id;

		optional<id_t> getId() { return id; }
		void clearInputData() { input.clear(); }
		void clearOutputData() { output.clear(); }

		virtual void call(Body* caller = nullptr, Behavior* behavior = nullptr) {
			ScArgs args(this, caller, behavior); // Construct ScArgs first
			if (std::holds_alternative<ScriptEvent>(handler)) {
				std::get<ScriptEvent>(handler)(args);
			}
			else if (std::holds_alternative<pybind11::object>(handler)) {
				try {
					pybind11::gil_scoped_acquire acquire;
					std::get<pybind11::object>(handler)(args);
				}
				catch (const pybind11::error_already_set& e) {
					std::cerr << "Python error in script execution: " << e.what() << std::endl;
				}
				catch (const std::exception& e) {
					std::cerr << "[Script::call] C++ Error during Python execution (ID: "
						<< (id.has_value() ? std::to_string(id.value()) : "N/A")
						<< "): " << e.what() << std::endl;
				}
			}
			// Optional: else { log warning: script has no callable }
		}
	protected:
		ScriptEventHandler handler;
	};
}