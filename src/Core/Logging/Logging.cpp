#include "../Engine/Engine.h"
#include "Logging.h"

namespace CGEngine {
	Logging::Logging(LogLevel level): logFilename("cgengine_log"), logLevel(level) {
		findUniqueFilepath();
	}

	Logging::~Logging() {
		writeQueue();
	}

	void Logging::findUniqueFilepath() {
		string systemTime = time.getSystemmTimeNowString();
		string dot = ".";
		filepath.append(logDirectory).append(logFilename).append(".").append(systemTime).append(".txt");
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

	void Logging::log(LogLevel level, string caller, string msg, vector<string> args, size_t precision) {
		if (level <= logLevel) {
			string formattedMsg = format(msg, args);
			//Build log level prompt
			string logLevelPrompt = string("[").append(levelToString(level)).append("] ");
			//Build caller prompt
			string callerPrompt = (caller == "") ? caller : string(caller).append(": ");
			//Build log message
			string logMsg = string(logLevelPrompt).append(callerPrompt).append(formattedMsg).append("\n");
			//Print and queue message
			cout << logMsg;
			queueMsg(LogEvent(time.getElapsedSec(), logMsg));
		}
	}

	string Logging::format(string msg, vector<string> args) {
		int index = 0;
		int argIndex = 0;
		stringstream msgStream;
		while (index < msg.size()) {
			if (msg[index] == '{' && index + 1 < msg.size() && msg[index + 1] == '}') {
				msgStream << args[argIndex];
				index += 2;
				argIndex++;
			}
			else {
				msgStream << msg[index];
				index++;
			}
		}
		return msgStream.str();
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
		if (logFile.is_open() && msgBuffer.size() > 0) {
			logFile.write(msgBuffer.data(), msgBuffer.size());
			logFile.close();
		} else {
			cout << "[ERROR] Can't open log file for writing.\n";
		}
	}

	void Logging::setPrecision(size_t precision) {
		this->precision = precision;
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
		return "INVALID";
	}

	void Logging::setLogLevel(LogLevel level) {
		logLevel = level;
	}
}