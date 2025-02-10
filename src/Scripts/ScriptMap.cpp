#include "ScriptMap.h"
#include "../World/WorldInstance.h"

namespace CGEngine {
	ScriptMap::ScriptMap(Body* o) {
		owner = o;
	}

	void ScriptMap::initialize() {
		string namePrompt = (owner->getName() != "") ? "(" + owner->getName() + ")" : "";
		string idPrompt = owner->getId().has_value() ? "[" + to_string(owner->getId().value()) + "]" : "";
		ownerName = "Body" + idPrompt + namePrompt;
	}

	size_t ScriptMap::addScript(string domainName, Script* script) {
		ScriptDomain* domain = getDomain(domainName);
		if (domain == nullptr) {
			domain = addDomain(domainName);
		}
		if (domain != nullptr) {
			size_t scriptId = domain->addScript(script);
			return scriptId;
		}
		return 0U;
	}

	void ScriptMap::removeScript(string domainName, size_t scriptId, bool shouldDelete) {
		if (ScriptDomain* domain = getDomain(domainName)) {
			domain->removeScript(scriptId);
			if (shouldDelete && domain->isEmpty()) {
				deleteDomain(domain);
			}
		}
	}

	void ScriptMap::removeScript(string domainName, Script* script, bool shouldDelete) {
		if (ScriptDomain* domain = getDomain(domainName)) {
			domain->removeScript(script);
			if (shouldDelete && domain->isEmpty()) {
				deleteDomain(domain);
			}
		}
	}

	void ScriptMap::eraseScript(string domainName, size_t scriptId, bool shouldDelete) {
		if (ScriptDomain* domain = getDomain(domainName)) {
			domain->eraseScript(scriptId);
			if (shouldDelete && domain->isEmpty()) {
				deleteDomain(domain);
			}
		}
	}
	void ScriptMap::eraseScript(string domainName, Script* script, bool shouldDelete) {
		if (ScriptDomain* domain = getDomain(domainName)) {
			domain->eraseScript(script);
			if (shouldDelete && domain->isEmpty()) {
				deleteDomain(domain);
			}
		}
	}

	void ScriptMap::clearDomain(string domainName) {
		log("Clearing Domain('" + domainName + "')");
		if (ScriptDomain* domain = getDomain(domainName)) {
			domain->clear();
		}
	}

	void ScriptMap::clear() {
		deleteDomains();
		//Domains have been deleted, so clear their pointers
		domains.clear();
	}

	void ScriptMap::callDomain(string domainName, bool logUpdate) {
		if (ScriptDomain* domain = getDomain(domainName)) {
			if (domainName != onUpdateEvent || logUpdate) {
				log("Calling Domain('" + domain->getName() + "')");
			}
			domain->callDomain(owner);
		}
	}

	void ScriptMap::callScript(string domainName, size_t scriptId) {
		if (ScriptDomain* domain = getDomain(domainName)) {
			log("Calling Script[" + to_string(scriptId) + "]('" + domain->getName() + "')");
			domain->callScript(scriptId, owner);
		}
	}

	void ScriptMap::callDomainWithData(string domainName, DataStack input, bool logUpdate) {
		if (ScriptDomain* domain = getDomain(domainName)) {
			if (domainName != onUpdateEvent || logUpdate) {
				log("Calling Domain('" + domain->getName() + "')");
			}
			domain->callDomain(owner, input);
		}
	}

	void ScriptMap::callScriptWithData(string domainName, size_t scriptId, DataStack input) {
		if (ScriptDomain* domain = getDomain(domainName)) {
			log("Calling Script[" + to_string(scriptId) + "]('" + domain->getName() + "')");
			domain->callScript(scriptId, owner,  input);
		}
	}

	void ScriptMap::deleteDomain(string domainName) {
		if (ScriptDomain* domain = getDomain(domainName)) {
			deleteDomain(domain);
		}
	}

	ScriptDomain* ScriptMap::addDomain(string domainName) {
		domains[domainName] = new ScriptDomain(domainName, ownerName);
		log("Added Domain('" + domainName + "')");
		return domains[domainName];
	}

	ScriptDomain* ScriptMap::getDomain(string domainName) {
		if (domains.find(domainName) != domains.end()) {
			return domains[domainName];
		}
		return nullptr;
	}

	void ScriptMap::deleteDomains() {
		for (auto iterator = domains.begin(); iterator != domains.end(); iterator++) {
			ScriptDomain* domain = (*iterator).second;
			//Refunds script ids, erases them from its map & deletes them, then clear the map. Prints a warning if scripts remain
			domain->deleteDomain();
		}
	}

	void ScriptMap::deleteDomain(ScriptDomain* domain) {
		if (domain != nullptr) {
			string domainName = domain->getName();
			//Find and erase the domain from the domains map
			if (domains.find(domainName) != domains.end()) domains.erase(domainName);
			//Refunds script ids, erase them from the domain's scripts map & delete them, then clear the map. Prints a warning if scripts remain
			domain->deleteDomain();
		}
	}

	void ScriptMap::log(string msg) {
		if (!logging.willLog(logLevel)) return;
		logging(logLevel, ownerName, msg);
	}
}