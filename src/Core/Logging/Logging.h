#pragma once

#include <iostream>
#include <queue>
#include <fstream>
#include <filesystem>
using std::string;
using std::cout;
using std::queue;
using std::ofstream;
using std::ios;
namespace fs = std::filesystem;

namespace CGEngine {
	enum LogLevel { LogError, LogWarn, LogInfo, LogDebug, LogDebug1, LogDebug2 };
	const string logLevels[6] = { "ERROR", "WARN", "INFO", "DEBUG", "DEBUG1", "DEBUG2" };
	struct LogEvent {
		LogEvent(sec_t timestamp, string msg) :timestamp(timestamp), msg(msg) {};
		sec_t timestamp;
		string msg;
	};

	class Logging {
	public:
		Logging(LogLevel level = LogLevel::LogWarn);
		~Logging();
		string logFilename;
		void operator()(LogLevel level, string caller, string msg, bool newLine = true, bool toFile = true);
		void operator()(string msg, LogLevel level = LogLevel::LogInfo, string caller = "", bool newLine = true, bool toFile = true);
		void operator()(string msg, string caller, LogLevel level, bool newLine = true, bool toFile = true);
		bool willLog(LogLevel level);
		void setActive(bool enabled);
		bool getActive();
		queue<LogEvent> logQueue;
		void queueMsg(LogEvent msgEvt);
		void writeQueue();
	protected:
		static LogLevel logLevel;
	private:
		string filepath;
		bool active = true;
		void log(LogLevel level, string caller, string msg, bool newLine = true, bool toFile = true);
		void findUniqueFilepath();
		string levelToString(LogLevel level);
	};
}