#include "ScriptDomain.h"
#include "../World/WorldInstance.h"

namespace CGEngine {
    ScriptDomain::ScriptDomain(string name, string bodyName) {
        domainName = name;
        ownerName = bodyName;
    }

    ScriptDomain::~ScriptDomain() {
        clear();
        for (const auto& [key, value] : scripts) {
            Script* script = value;
            delete script;
        }
    }

    string ScriptDomain::getName() {
        return domainName;
    }

    size_t ScriptDomain::addScript(Script* script) {
        size_t id = domainIds.receive(&script->id);
        scripts[id] = script;
        log("Added Script[" + to_string(id) + "]('" + domainName + "')");
        return id;
    }

    Script* ScriptDomain::getScript(size_t scriptId) {
        auto iterator = scripts.find(scriptId);
        if (iterator != scripts.end()) {
            return (*iterator).second;
        }
        return nullptr;
    }

    void ScriptDomain::removeScript(size_t scriptId, bool shouldLog) {
        if (Script* script = getScript(scriptId)) {
            //Refund the script id & erase it from the map (if it has an id)
            removeScript(script, shouldLog);
        }
    }

    void ScriptDomain::removeScript(Script* script, bool shouldLog) {
        //Ignore scripts with no id
        if (script->id.has_value()) {
            size_t scriptId = script->id.value();
            if (shouldLog) {
                log("Removed Script[" + to_string(scriptId) + "]('" + domainName + "')");
            }
            //Refund the script id and erase it from the scripts map
            domainIds.refund(&script->id);
            scripts.erase(scriptId);
        }
    }

    void ScriptDomain::eraseScript(size_t scriptId) {
        //Ignore invalid script ids
        if (Script* script = getScript(scriptId)) {
            //Refund the script id & erase it from the map
            removeScript(script, false);
            //Delete it
            deleteScript(script, scriptId);
        }
    }

    void ScriptDomain::eraseScript(Script* script) {
        //Ignore null scripts (and scripts with no id)
        if (script != nullptr) {
            optional<id_t> scriptId = script->id;
            //Refund the script id & erase it from the map (if it has an id)
            removeScript(script, false);
            //Delete it
            deleteScript(script, scriptId);
        }
    }

    bool ScriptDomain::isEmpty() {
        return scripts.size() <= 0;
    }

    void ScriptDomain::clear() {
        vector<size_t> keys = getScriptIds();
        for (int i = 0; i < keys.size(); i++) {
            auto iterator = scripts.find(keys[i]);
            if (iterator != scripts.end()) {
                Script* script = (*iterator).second;
                removeScript(script);
            }
        }
        scripts.clear();
    }

    vector<size_t> ScriptDomain::getScriptIds() {
        vector<size_t> keys;
        for (const auto& [key, value] : scripts) { keys.push_back(key); }
        return keys;
    }

    void ScriptDomain::callDomain(Body* caller) {
        vector<size_t> keys = getScriptIds();

        for (int i = 0; i < keys.size(); i++) {
            auto iterator = scripts.find(keys[i]);
            if (iterator != scripts.end()) {
                Script* script = (*iterator).second;
                script->call(caller);
                if (world->isDeleted(caller)) return;
            }
        }
    }

    void ScriptDomain::callDomain(Body* caller, optional<DataMap> input) {
        vector<size_t> keys = getScriptIds();

        for (int i = 0; i < keys.size(); i++) {
            auto iterator = scripts.find(keys[i]);
            if (iterator != scripts.end()) {
                Script* script = (*iterator).second;
                if (input != nullopt) {
                    script->setInput(input.value());
                }
                script->call(caller);
                if (world->isDeleted(caller)) return;
            }
        }
    }

    void ScriptDomain::setDomainInput(DataMap data) {
        vector<size_t> keys = getScriptIds();

        for (int i = 0; i < keys.size(); i++) {
            auto iterator = scripts.find(keys[i]);
            if (iterator != scripts.end()) {
                Script* script = (*iterator).second;
                script->setInput(data);
            }
        }
    }
    void ScriptDomain::setScriptInput(size_t scriptId, DataMap data) {
        auto iterator = scripts.find(scriptId);
        if (iterator != scripts.end()) {
            Script* script = (*iterator).second;
            script->setInput(data);
        }
    }

    void ScriptDomain::callScript(size_t scriptId, Body* caller) {
        if (Script* script = getScript(scriptId)) {
            script->call(caller);
        }
    }

    void ScriptDomain::callScript(size_t scriptId, Body* caller, DataMap input) {
        if (Script* script = getScript(scriptId)) {
            script->setInput(input);
            script->call(caller);
        }
    }

    void ScriptDomain::deleteDomain() {
        vector<size_t> keys = getScriptIds();
        for (int i = 0; i < keys.size(); i++) {
            auto iterator = scripts.find(keys[i]);
            if (iterator != scripts.end()) {
                Script* script = (*iterator).second;
                eraseScript(script);
            }
        }
        if (scripts.size() > 0) {
            logging(LogLevel::LogWarn, "ScriptDomainError", "Not all scripts of domain were deleted");
            clear();
        }
        log("Deleted Domain('" + domainName + "')");
        delete this;
    }

    void ScriptDomain::deleteScript(Script* script, optional<id_t> scriptId) {
        if (script == nullptr) return;
        if (script->id.has_value()) {
            log("Deleted Script[" + to_string(script->id.value()) + "]('" + domainName + "')");
        }
        else if (scriptId.has_value()) {
            log("Deleted Script[" + to_string(scriptId.value()) + "]('" + domainName + "')");
        }
        delete script;
    }

    void ScriptDomain::log(string msg) {
        if (!logging.willLog(logLevel)) return;
        logging(LogLevel::LogWarn, ownerName, msg);
    }
}