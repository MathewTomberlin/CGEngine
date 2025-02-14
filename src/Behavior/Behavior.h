#pragma once

#include <stack>
#include <optional>
#include <any>
#include "../Scripts/ScriptMap.h"
#include "../Body/Body.h"

using namespace std;

namespace CGEngine {
	class Behavior {
	public:
		Behavior(Body* owning, string name = "");

		id_t addScript(string domain, Script* script);
		void removeScript(string domain, id_t scriptId, bool shouldDelete = false);
		void addScriptEventsByDomain(map<string, ScriptEvent> sc);
		void callDomain(string domain);
		void callDomainWithData(string domain, DataMap data);

		void setInput(DataMap in);
		void setOutput(DataMap out);
		void setProcess(DataMap d);
		DataMap getInput();
		DataMap getOutput();
		DataMap getProcess();

		template<typename T>
		T getInputData(string key) {
			return input.getData<T>(key);
		}

		template<typename T>
		T getOutputData(string key) {
			return output.getData<T>(key);
		}

		template<typename T>
		T getProcessData(string key) {
			return process.getData<T>(key);
		}

		void setInputData(string key, any value);
		void setOutputData(string key, any value);
		void setProcessData(string key, any value);
		id_t getId();
		Body* getOwner();
		string getName();
	private:
		string displayName = "";
		Body* owner;
		optional<id_t> behaviorId = nullopt;
		ScriptMap scripts;

		DataMap input;
		DataMap process;
		DataMap output;
	};
}