#pragma once

#include "../Types/UniqueIntegerStack.h"
#include "Timer.h"
#include "../Types/Types.h"
#include "../Logging/Logging.h"
#include "../Types/UniqueDomain.h"
#include "../Engine/EngineSystem.h"

namespace CGEngine {
	typedef optional<size_t> timerId_t;

	class Body;
	class Script;

	class TimerMap : public EngineSystem {
	public:
		TimerMap();
		/// <summary>
		/// Set a timer on the indicated body for the duration, calling the onCompleteEvent when the duration expires and resetting
		/// the timer up to loopCount timers when it is completed
		/// </summary>
		/// <param name="body">The body to set the timer on and the body passed to the Script when the timer expires</param>
		/// <param name="duration">The time before the timer expires</param>
		/// <param name="onCompleteScript">The Script to be called when the timer expires</param>
		/// <param name="loopCount">The number of times to reset the timer</param>
		/// <param name="timerDisplayName">The printed display name of the timer</param>
		/// <returns></returns>
		timerId_t setTimer(Body* body, sec_t duration, Script* onCompleteScript, int loopCount = 0, string timerDisplayName = "");
		/// <summary>
		/// Cancel the timer with the indicated timer id on the Body
		/// </summary>
		/// <param name="body">The Body to cancel the timer on</param>
		/// <param name="timerId">The id of the timer to cancel</param>
		void cancelTimer(Body* body, size_t timerId);
		/// <summary>
		/// Cancel the timer with the indicated timer id on the Body
		/// </summary>
		/// <param name="body">The Body to cancel the timer on</param>
		/// <param name="timerId">The id of the timer to cancel</param>
		void cancelTimer(Body* body, timerId_t* timerId);
		/// <summary>
		/// Delete all timers on the Body (but doesn't erase them from the TimerMap)
		/// </summary>
		/// <param name="body">The Body to delete the timers on</param>
		void deleteTimers(Body* body);
		/// <summary>
		/// Clear all timer entries (but doesn't delete them)
		/// </summary>
		void clear();
	private:
		/// <summary>
		/// Delete the timer with the indicated id and erase it from the TimerMap
		/// </summary>
		/// <param name="timerId">The id of the timer to delete</param>
		void deleteTimer(size_t timerId);
		/// <summary>
		/// A unique id list of timers
		/// </summary>
		UniqueDomain<size_t, Timer*> timers = UniqueDomain<size_t, Timer*>(1000U);
	};
}