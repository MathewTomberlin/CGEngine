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
	private:
		friend class World;
		string displayName = "";
		Script* loadEvent;
	};
}