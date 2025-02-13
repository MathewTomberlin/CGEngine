#pragma once

#include "../Types/DataStack.h"
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

		//template <typename T>
		//T pullOutInput() {
		//	static T test;
		//	return input.pullOut<T>().value_or(test);
		//}
		//
		//template <typename T>
		//optional<T> pullOutOptionalInput() {
		//	static T test;
		//	return input.pullOut<T>();
		//}
		//
		//template <typename T>
		//T peekInput() {
		//	static T test;
		//	return input.peek<T>().value_or(test);
		//}
		//
		//template <typename T>
		//optional<T> peekOptionalInput() {
		//	static T test;
		//	return input.peek<T>();
		//}
		//
		//template <typename T>
		//T pullOutOutput() {
		//	static T test;
		//	return output.pullOut<T>().value_or(test);
		//}
		//
		//template <typename T>
		//optional<T> pullOutOptionalOutput() {
		//	static T test;
		//	return output.pullOut<T>();
		//}
		//
		//template <typename T>
		//T peekOutput() {
		//	static T test;
		//	return output.peek<T>().value_or(test);
		//}
		//
		//template <typename T>
		//optional<T> peekOptionalOutput() {
		//	static T test;
		//	return output.peek<T>();
		//}
		//
		//template <typename T>
		//T* pullOutInputPtr() {
		//	return input.pullOut<T*>().value_or(nullptr);
		//}
		//
		//template <typename T>
		//T* peekInputPtr() {
		//	return input.peek<T*>().value_or(nullptr);
		//}
		//
		//template <typename T>
		//T* pullOutOutputPtr() {
		//	return output.pullOut<T*>().value_or(nullptr);
		//}
		//
		//template <typename T>
		//T* peekOutputPtr() {
		//	return output.peek<T*>().value_or(nullptr);
		//}

		void setInput(DataMap stack) { input = stack; }
		void setOutput(DataMap stack) { output = stack; }
		DataMap getInput() { return input; }
		DataMap getOutput() { return output;  }
	private:
		DataMap input;
		DataMap output;
	};
}