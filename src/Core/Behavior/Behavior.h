#pragma once

#include <stack>
#include <optional>
#include <any>
#include "../Scripts/ScriptMap.h"
#include "../Body/Body.h"
#include "../Types/DataControllers/InputDataController.h"
#include "../Types/DataControllers/OutputDataController.h"
#include "../Types/DataControllers/ProcessDataController.h"

using namespace std;

namespace CGEngine {
	class Behavior : public InputDataController, public OutputDataController, public ProcessDataController {
	public:
		Behavior(Body* owning, string name = "");

		id_t addScript(string domain, Script* script);
		void removeScript(string domain, id_t scriptId, bool shouldDelete = false);
		void addScriptEventsByDomain(map<string, ScriptEvent> sc);
		void callDomain(string domain);
		void callDomainWithData(string domain, DataMap data);

		id_t getId();
		Body* getOwner();
		string getName();
	private:
		string displayName = "";
		Body* owner;
		optional<id_t> behaviorId = nullopt;
		ScriptMap scripts;
	};
}