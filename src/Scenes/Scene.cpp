#include "Scene.h"
#include "../World/WorldInstance.h"
#include <queue>

namespace CGEngine {
	Scene::Scene(ScriptEvent loadEvent, string name) : displayName(name), loadEvent(new Script(loadEvent)) { };

	string Scene::getDisplayName() {
		return displayName;
	};

	void Scene::setDisplayName(string name) {
		displayName = name;
	}

	void Scene::load(Body* sceneRoot) {
		loadEvent->call(sceneRoot);
	}

	DataMap Scene::getInput() {
		return loadEvent->getInput();
	}

	DataMap Scene::getOutput() {
		return loadEvent->getOutput();
	}

	void Scene::setInput(DataMap in) {
		loadEvent->setInput(in);
	}

	void Scene::setOutput(DataMap out) {
		loadEvent->setOutput(out);
	}
}