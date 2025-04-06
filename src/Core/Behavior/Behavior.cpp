#include "Behavior.h"

namespace CGEngine {
	Behavior::Behavior(Body* owning, string displayName) : scripts(owning), displayName(displayName), owner(owning) {
		if (owner != nullptr) {
			behaviorId = owner->addBehavior(this);
		}
		scripts.initialize();
	};

	id_t Behavior::addScript(string domain, Script* script) {
		return scripts.addScript(domain, script);
	}

	void Behavior::removeScript(string domain, id_t scriptId, bool shouldDelete) {
		scripts.removeScript(domain, scriptId, shouldDelete);
	}

	void Behavior::addScriptEventsByDomain(map<string, ScriptEvent> scripts) {
		for (auto iterator = scripts.begin(); iterator != scripts.end(); ++iterator) {
			string domain = (*iterator).first;
			addScript((*iterator).first, new Script((*iterator).second));
		}
	}

	void Behavior::callDomain(string domain) {
		scripts.callDomain(domain, this);
	}

	void Behavior::callStaticDomain(StaticScriptDomain domainId) {
		if (domainId < 0 || domainId > 2) return;
		scripts.callStaticDomain(domainId, this);
	}

	void Behavior::callDomainWithData(string domain, DataMap data) {
		scripts.callDomainWithData(domain, this, data);
	}

	
	optional<id_t> Behavior::getId() {
		return behaviorId.value();
	}

	Body* Behavior::getOwner() {
		return owner;
	}

	string Behavior::getName() {
		return displayName;
	}
}