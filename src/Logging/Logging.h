#pragma once

#include <iostream>
using std::string;
using std::cout;

namespace CGEngine {
	enum LogLevel { LogError, LogWarn, LogInfo, LogDebug, LogDebug1, LogDebug2 };
	const string logLevels[6] = { "ERROR", "WARN", "INFO", "DEBUG", "DEBUG1", "DEBUG2" };

	class Logging {
	public:
		Logging(LogLevel level = LogLevel::LogWarn);
		void operator()(LogLevel level, string caller, string msg, bool newLine = true);
		bool willLog(LogLevel level);
		void setActive(bool enabled);
		bool getActive();
	protected:
		static LogLevel logLevel;
	private:
		bool active = true;
		void log(LogLevel level, string caller, string msg, bool newLine = true);
	};
}