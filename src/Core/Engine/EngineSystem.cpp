#include "EngineSystem.h"
#include "Engine.h"

namespace CGEngine {
	EngineSystem::EngineSystem() {
		
	}

	LogLevel EngineSystem::getLogLevel() {
		return logLevel;
	}
	void EngineSystem::setLogLevel(LogLevel level) {
		logLevel = level;
	}

	string EngineSystem::getSystemName() {
		return systemName;
	}
	void EngineSystem::setSystemName(string name) {
		systemName = name;
	}

	void EngineSystem::init() {
		systemName = typeid(*this).name();
		systemName = systemName.substr(systemName.find_last_of(':') + 1, systemName.size());
	}
}