#include "Behavior.h"

namespace CGEngine {
	Behavior::Behavior(Body* owning) : scripts(owning) {
		owner = owning;
		behaviorId = owner->addBehavior(this);
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

	void Behavior::setInput(DataMap in) {
		input = in;
	}

	void Behavior::setOutput(DataMap out) {
		output = out;
	}

	void Behavior::setProcess(DataMap d) {
		process = d;
	}

	DataMap Behavior::getInput() {
		return input;
	}

	DataMap Behavior::getOutput() {
		return output;
	}

	DataMap Behavior::getProcess() {
		return process;
	}

	void Behavior::setInputData(string key, any value) {
		input.setData(key, value);
	}

	void Behavior::setOutputData(string key, any value) {
		output.setData(key, value);
	}

	void Behavior::setProcessData(string key, any value) {
		process.setData(key, value);
	}

	id_t Behavior::getId() {
		return behaviorId.value_or(0U);
	}

	Body* Behavior::getOwner() {
		return owner;
	}
}