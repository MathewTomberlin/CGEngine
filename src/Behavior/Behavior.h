#pragma once

#include <stack>
#include "../Scripts/ScriptMap.h"

using namespace std;
class Body;

namespace CGEngine {
	class Behavior {
	public:
		Behavior(Body* owning) : scripts(owning) {
			owner = owning; 
		};

		id_t addScript(string domain, Script* script) {
			return scripts.addScript(domain, script);
		}

		void removeScript(string domain, id_t scriptId, bool shouldDelete = false) {
			scripts.removeScript(domain, scriptId, shouldDelete);
		}

		void addScriptEventsByDomain(map<string, ScriptEvent> sc) {
			for (auto iterator = sc.begin(); iterator != sc.end(); ++iterator) {
				string domain = (*iterator).first;
				addScript((*iterator).first, new Script((*iterator).second));
			}
		}

		void callDomain(string domain) {
			scripts.callDomain(domain, this);
		}

		void callDomainWithData(string domain, DataMap data) {
			scripts.callDomainWithData(domain, this, data);
		}

		void setInput(DataMap in) {
			input = in;
		}

		void setOutput(DataMap out) {
			output = out;
		}

		void setProcess(DataMap d) {
			process = d;
		}

		DataMap getInput() {
			return input;
		}

		DataMap getOutput() {
			return output;
		}

		DataMap getProcess() {
			return process;
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
		T getProcessData(string key) {
			return process.getData<T>(key);
		}

		void setInputData(string key, any value) {
			input.setData(key, value);
		}

		void setOutputData(string key, any value) {
			output.setData(key, value);
		}

		void setProcessData(string key, any value) {
			process.setData(key, value);
		}


	private:
		Body* owner;
		ScriptMap scripts;

		DataMap input;
		DataMap process;
		DataMap output;
	};
}