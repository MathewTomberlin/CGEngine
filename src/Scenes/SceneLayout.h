#pragma once

#include <functional>
#include "../Types/Types.h"
#include "../Scripts/Script.h"
using namespace std;

namespace CGEngine {
	class SceneLayout {
	public:
		SceneLayout(vector<Script*> layouts = {}) {
			for (Script* layoutScript : layouts) {
				addScript(layoutScript);
			}
		};

		void applyLayout() {
			for (auto layoutPair : scripts) { 
				Script* script = layoutPair.second;
				script->scriptEvent(ScArgs(script, nullptr));
			}
		}

		id_t addScript(Script* script) {
			optional<id_t> scriptId = nullopt;
			scripts[scriptIds.receive(&scriptId)] = script;
			return scriptId.value();
		}

		void eraseScript(id_t scriptId) {
			if (scripts.find(scriptId) != scripts.end()) {
				scripts.erase(scripts.find(scriptId));
			}
		};
	private:
		map<id_t, Script*> scripts;
		UniqueIntegerStack<id_t> scriptIds = UniqueIntegerStack<id_t>(100);
	};
}