#pragma once

#include <string>
#include "../Types/Types.h"
using namespace std;

namespace CGEngine {
	class EngineSystem {
	public:
		EngineSystem();
		virtual ~EngineSystem() = default;
		string getSystemName();
		void setSystemName(string name);
		LogLevel getLogLevel();
		void setLogLevel(LogLevel level);
		void init();
	protected:
		string systemName;
		LogLevel logLevel = LogLevel::LogWarn;
		bool active = true;
	};
}