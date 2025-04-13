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
		Script(ScriptEvent evt);
		Script(pybind11::object callable);

		virtual ~Script() = default;

		ScriptEvent scriptEvent;
		pybind11::object pyScriptEvent = pybind11::none();
		optional<size_t> id;

		optional<id_t> getId() { return id; }
		void clearInputData() { input.clear(); }
		void clearOutputData() { output.clear(); }

		virtual void call(Body* caller = nullptr, Behavior* behavior = nullptr) {
			ScArgs args(this, caller, behavior); // Construct ScArgs first

			if (pyScriptEvent && !pyScriptEvent.is_none()) { // Check if Python callable is set
				try {
					pybind11::gil_scoped_acquire acquire; // Acquire GIL before Python call
					pyScriptEvent(args); // Call the Python object, passing ScArgs (requires ScArgs binding)
					//TODO: Potential future optimization? Can somehow not allowing GIL to be destroyed optimize?
					// GIL released automatically by destructor of 'acquire'
				}
				catch (const pybind11::error_already_set& e) {
					// Log the Python error (including traceback)
					std::cerr << "[Script::call] Python Error executing script (ID: "
						<< (id.has_value() ? std::to_string(id.value()) : "N/A")
						<< "): " << e.what() << std::endl;
					// Optional: Add more robust error handling (e.g., disable script)
				}
				catch (const std::exception& e) {
					std::cerr << "[Script::call] C++ Error during Python execution (ID: "
						<< (id.has_value() ? std::to_string(id.value()) : "N/A")
						<< "): " << e.what() << std::endl;
				}
			}
			else if (scriptEvent) { // Fallback to C++ lambda if it exists
				scriptEvent(args);
			}
			// Optional: else { log warning: script has no callable }
		}

		/// <summary>
		/// A Script shouldn't have both a Python ScriptEvent and a C++ ScriptEvent
		/// </summary>
		/// <param name="callable"></param>
		void setPyScriptEvent(pybind11::object callable);
	};
}