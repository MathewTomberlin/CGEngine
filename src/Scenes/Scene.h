#pragma once

#include <string>
#include "../Body/Body.h"
#include "../World/Renderer.h"
#include <algorithm>
#include "SceneLayout.h"
using namespace std;

namespace CGEngine {
	class Scene {
	public:
		Scene(ScriptEvent loadEvent = [](ScArgs args) {}, string name = "NewScene");
		string getDisplayName();
		void setDisplayName(string name);
		void load(Body* sceneRoot = nullptr);
		void setInput(DataStack in);
		void setOutput(DataStack out);
		DataStack getInput();
		DataStack getOutput();
		Script* loadEvent;
		string displayName = "";
	private:
		friend class World;
		DataStack input;
		DataStack output;
	};
}