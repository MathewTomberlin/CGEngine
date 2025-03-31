#pragma once

#include <iostream>
#include <queue>
#include <fstream>
#include <filesystem>
#include <cmath>
#include "../Types/Types.h"
#include "../Engine/EngineSystem.h"
using std::string;
using std::cout;
using std::queue;
using std::ofstream;
using std::ios;
namespace fs = std::filesystem;

namespace CGEngine {
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

		//Numeric to string
		template <typename T>
		enable_if_t<is_arithmetic_v<T>, string> argToString(T value) {
			stringstream sstream;
			sstream << fixed << setprecision(precision) << value;
			return sstream.str();
		}
		//String to string
		template <typename T>
		enable_if_t<is_same_v<T, string>, string> argToString(const T& value) {
			return value;
		}
		//String literal to string
		template <size_t N>
		string argToString(const char(&value)[N]) {
			return string(value);
		}
		//Char* to string
		string argToString(const char* value) {
			return string(value);
		}
		//Char to string
		string argToString(const char value) {
			return string(1,value);
		}

		template <typename... Args>
		void operator()(LogLevel level, string caller, string msg, Args... args) {
			log(level, caller, msg, { argToString(forward<Args>(args))... });
		}

		template <typename... Args>
		void operator()(EngineSystem* system, LogLevel level, string msg, Args... args) {
			if(!system) return;
			if (level <= system->getLogLevel()) {
				log(level, system->getSystemName(), msg, {argToString(forward<Args>(args))...});
			}
		}

		bool willLog(LogLevel level);
		void setActive(bool enabled);
		bool getActive();
		queue<LogEvent> logQueue;
		void queueMsg(LogEvent msgEvt);
		void writeQueue();
		void setPrecision(size_t precision);
		void setLogLevel(LogLevel level);
	protected:
		LogLevel logLevel;
	private:
		string filepath;
		string logDirectory = "logs/";
		bool active = true;
		size_t precision = 2;
		void log(LogLevel level, string caller, string msg, vector<string> args, size_t precision = 2);
		string format(string msg, vector<string> args);
		void findUniqueFilepath();
		string levelToString(LogLevel level);
	};
}