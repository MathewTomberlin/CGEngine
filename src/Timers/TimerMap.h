#pragma once

#include "../Types/UniqueIntegerStack.h"
#include "Timer.h"
#include "../Types/Types.h"
#include "../Logging/Logging.h"
using namespace std;

namespace CGEngine {
	typedef optional<size_t> timerId_t;

	class Body;
	class Script;

	class TimerMap {
	public:
		timerId_t setTimer(Body* body, sec_t duration, Script* onCompleteEvent, int loopCount = 0, string timerDisplayName = "");
		void cancelTimer(Body* body, size_t timerId);
		void cancelTimer(Body* body, timerId_t* timerId);
		void deleteTimers(Body* body);
		LogLevel logLevel = LogLevel::LogInfo;
		void clear();
	private:
		void deleteTimer(size_t timerId);
		void log(string timerName, size_t timerId, string msg);
		map<size_t, Timer*> timers;
		UniqueIntegerStack<size_t> timerIds = UniqueIntegerStack<size_t>(100U);
	};
}