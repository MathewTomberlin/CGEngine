#include "Behavior.h"

namespace CGEngine {
	Behavior::Behavior(Body* owning, string name) : scripts(owning) {
		displayName = name;
		owner = owning;
		if (owner != nullptr) {
			behaviorId = owner->addBehavior(this);
		}
	};

	id_t Behavior::addScript(string domain, Script* script) {
		return scripts.addScript(domain, script);
	}

	void Behavior::removeScript(string domain, id_t scriptId, bool shouldDelete) {
		scripts.removeScript(domain, scriptId, shouldDelete);
	}

	void Behavior::addScriptEventsByDomain(map<string, ScriptEvent> sc) {
		for (auto iterator = sc.begin(); iterator != sc.end(); ++iterator) {
			string domain = (*iterator).first;
			addScript((*iterator).first, new Script((*iterator).second));
		}
	}

	void Behavior::callDomain(string domain) {
		scripts.callDomain(domain, this);
	}

	void Behavior::callDomainWithData(string domain, DataMap data) {
		scripts.callDomainWithData(domain, this, data);
	}

	
	id_t Behavior::getId() {
		return behaviorId.value_or(0U);
	}

	Body* Behavior::getOwner() {
		return owner;
	}

	string Behavior::getName() {
		return displayName;
	}
}