#include "ScriptController.h"
#include "../../Engine/Engine.h"
#include "../../Scripts/Script.h"

namespace CGEngine {
	id_t ScriptController::addScript(string domain, Script* script) {
		return scripts.addScript(domain, script);
	}

	void ScriptController::addScriptEventsByDomain(map<string, ScriptEvent> sc) {
		for (auto iterator = sc.begin(); iterator != sc.end(); ++iterator) {
			string domain = (*iterator).first;
			addScript((*iterator).first, new Script((*iterator).second));
		}
	}

	void ScriptController::removeScript(string domain, id_t scriptId, bool shouldDelete) {
		scripts.removeScript(domain, scriptId, shouldDelete);
	}

	id_t ScriptController::addStartScript(Script* script) {
		return scripts.addScript(onStartEvent, script);
	}

	id_t ScriptController::addUpdateScript(Script* script) {
		return scripts.addScript(onUpdateEvent, script);
	}

	id_t ScriptController::addDeleteScript(Script* script) {
		return scripts.addScript(onDeleteEvent, script);
	}

	void ScriptController::eraseScript(string domain, id_t scriptId, bool shouldDelete) {
		scripts.eraseScript(domain, scriptId, shouldDelete);
	}

	void ScriptController::eraseScript(string domain, Script* script, bool shouldDelete) {
		scripts.eraseScript(domain, script, shouldDelete);
	}

	void ScriptController::eraseStartScript(id_t scriptId, bool shouldDelete) {
		eraseScript(onStartEvent, scriptId, shouldDelete);
	}

	void ScriptController::eraseUpdateScript(id_t scriptId, bool shouldDelete) {
		eraseScript(onUpdateEvent, scriptId, shouldDelete);
	}

	void ScriptController::eraseDeleteScript(id_t scriptId, bool shouldDelete) {
		eraseScript(onDeleteEvent, scriptId, shouldDelete);
	}

	void ScriptController::deleteDomain(string domain) {
		scripts.deleteDomain(domain);
	}

	void ScriptController::clearDomain(string domain) {
		scripts.clearDomain(domain);
	}

	void ScriptController::callDomain(string domain) {
		scripts.callDomain(domain);
	}

	void ScriptController::callDomainWithData(string domain, DataMap data) {
		scripts.callDomainWithData(domain, nullptr, data);
	}
}