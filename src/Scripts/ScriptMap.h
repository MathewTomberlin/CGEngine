#pragma once

#include "ScriptDomain.h"
#include "../Logging/Logging.h"

namespace CGEngine {
	class ScriptMap {
	public:
		ScriptMap(Body* o);
		size_t addScript(string domainName, Script* script);
		void removeScript(string domainName, size_t scriptId, bool shouldDelete = false);
		void removeScript(string domainName, Script* script, bool shouldDelete = false);
		void eraseScript(string domainName, size_t scriptId, bool shouldDelete = false);
		void eraseScript(string domainName, Script* script, bool shouldDelete = false);
		void clearDomain(string domainName);
		void clear();
		void callDomain(string domainName,  bool logUpdate = false);
		void callScript(string domainName, size_t scriptId);
		void callDomainWithData(string domainName, DataMap input = DataMap(), bool logUpdate = false);
		void callScriptWithData(string domainName, size_t scriptId, DataMap input = DataMap());
		void deleteDomain(string domainName);
		LogLevel logLevel = LogLevel::LogInfo;
	private:
		friend class Body;
		Body* owner = nullptr;
		string ownerName = "";
		map<string, ScriptDomain*> domains;
		void initialize();
		ScriptDomain* addDomain(string domainName);
		ScriptDomain* getDomain(string domainName);
		void deleteDomains();
		void deleteDomain(ScriptDomain* domain);
		void log(string msg);
	};
}