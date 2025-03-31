#include "../Engine/Engine.h"
#include "Logging.h"

namespace CGEngine {
	LogLevel Logging::logLevel;

	Logging::Logging(LogLevel level): logFilename("cgengine_log") {
		findUniqueFilepath();

		logLevel = level;
	}

	Logging::~Logging() {
		writeQueue();
	}

	void Logging::findUniqueFilepath() {
		string filetype = ".txt";
		string logDirectory = "logs/";
		string systemTime = time.getSystemmTimeNowString();
		string dot = ".";
		filepath.reserve(logDirectory.size() + logFilename.size() + dot.size() + systemTime.size() + filetype.size());
		filepath.append(logDirectory).append(logFilename).append(dot).append(systemTime).append(filetype);
	}

	void Logging::operator()(LogLevel level, string caller, string msg, bool newLine, bool toFile) {
		log(level, caller, msg, newLine, toFile);
	}

	void Logging::operator()(string msg, LogLevel level, string caller, bool newLine, bool toFile) {
		log(level, caller, msg, newLine, toFile);
	}

	void Logging::operator()(string msg, string caller, LogLevel level, bool newLine, bool toFile) {
		log(level, caller, msg, newLine, toFile);
	}

	bool Logging::willLog(LogLevel level) {
		return active && level <= logLevel;
	}

	void Logging::setActive(bool enabled) {
		active = enabled;
	}

	bool Logging::getActive() {
		return active;
	}

	void Logging::log(LogLevel level, string caller, string msg, bool newLine, bool toFile) {
		if (level <= logLevel) {
			string logMsg;
			//Build log level prompt
			string logLevelPrompt;
			string leftBracket = "[";
			string rightBracket = "]";
			string logLevel = levelToString(level);
			logLevelPrompt.reserve(leftBracket.size() + logLevel.size() + rightBracket.size());
			logLevelPrompt.append(leftBracket).append(logLevel).append(rightBracket);
			//Build caller prompt
			string prompter = ":";
			string space = " ";
			string callerPrompt;
			if (caller != "") {
				callerPrompt.reserve(space.size() + caller.size() + prompter.size() + space.size());
				callerPrompt.append(space).append(caller).append(prompter).append(space);
			}
			else {
				callerPrompt = " ";
			}
			//Build newline
			string nl = (newLine) ? "\n" : "";
			//Build log message
			logMsg.reserve(logLevelPrompt.size() + callerPrompt.size() + msg.size() + nl.size());
			logMsg.append(logLevelPrompt).append(callerPrompt).append(msg).append(nl);
			//Print and queue message
			cout << logMsg;
			queueMsg(LogEvent(time.getElapsedSec(), logMsg));
		}
	}

	void Logging::queueMsg(LogEvent msgEvt) {
		logQueue.push(msgEvt);
	}

	void Logging::writeQueue() {
		vector<char> msgBuffer;
		while (logQueue.size()) {
			LogEvent logEvt = logQueue.front();
			logQueue.pop();
			msgBuffer.insert(msgBuffer.end(),logEvt.msg.begin(), logEvt.msg.end());
		}
		ofstream logFile(filepath, ios::binary);
		if (logFile.is_open()) {
			logFile.write(msgBuffer.data(), msgBuffer.size());
			logFile.close();
		} else {
			cout << "[ERROR] Can't open log file for writing.\n";
		}
	}

	string Logging::levelToString(LogLevel level) {
		switch (level) {
		case LogLevel::LogInfo:
			return "INFO";
		case LogLevel::LogWarn:
			return "WARN";
		case LogLevel::LogError:
			return "ERROR";
		case LogLevel::LogDebug:
			return "DEBUG";
		case LogLevel::LogDebug1:
			return "DEBUG1";
		case LogLevel::LogDebug2:
			return "DEBUG2";
		}
	}
}