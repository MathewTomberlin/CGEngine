#pragma once

#include <string>
#include "../Body/Body.h"
#include "../World/Renderer.h"
#include <algorithm>
using namespace std;

namespace CGEngine {
	class Scene {
	public:
		Scene(ScriptEvent loadEvent = [](ScArgs args) {}, string name = "NewScene");
		string getDisplayName();
		void setDisplayName(string name);
		void load(Body* sceneRoot = nullptr);
		void setInput(DataMap in);
		void setOutput(DataMap out);
		DataMap getInput();
		DataMap getOutput();

		template<typename T>
		T getInputData(string key) {
			return input.getData<T>(key);
		}

		template<typename T>
		T getOutputData(string key) {
			return output.getData<T>(key);
		}

		void setInputData(string key, any value);
		void setOutputData(string key, any value);
		Script* loadEvent;
		string displayName = "";
	private:
		friend class World;
		DataMap input;
		DataMap output;
	};
}