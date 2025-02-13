#pragma once

#include <functional>
#include <optional>
#include "../Types/Types.h"
#include "../Types/DataMap.h"
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

	class Script {
	public:
		Script(ScriptEvent evt);

		ScriptEvent scriptEvent;
		optional<size_t> id;

		virtual void call(Body* caller = nullptr, Behavior* behavior = nullptr) {
			scriptEvent(ScArgs(this, caller, behavior));
		}

		template<typename T>
		T getInputData(string key) {
			return input.getData<T>(key);
		}

		template<typename T>
		T getOutputData(string key) {
			return output.getData<T>(key);
		}

		template<typename T>
		T* getInputDataPtr(string key) {
			return input.getDataPtr<T>(key);
		}

		template<typename T>
		T* getOutputDataPtr(string key) {
			return output.getDataPtr<T>(key);
		}

		void setInputData(string key, any value) {
			input.setData(key, value);
		}

		void setOutputData(string key, any value) {
			output.setData(key, value);
		}

		void setInput(DataMap stack) { input = stack; }
		void setOutput(DataMap stack) { output = stack; }
		DataMap getInput() { return input; }
		DataMap getOutput() { return output;  }
	private:
		DataMap input;
		DataMap output;
	};
}