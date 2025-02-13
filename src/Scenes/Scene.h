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
		Script* loadEvent;
		string displayName = "";
	private:
		friend class World;
		DataMap input;
		DataMap output;
	};
}