#pragma once

#include "SFML/Graphics.hpp"
#include "../Types/Types.h"
using namespace sf;

namespace CGEngine {
	class GlobalTime {
	public:
		sec_t getLastFrameSec();
		sec_t getElapsedSec();
		sec_t getDeltaSec() const;
		void updateDeltaTime();
	private:
		Clock runningClock;
		Clock frameClock;
		sec_t deltaSec = 0.f;
		sec_t currentFrameSec = 0.0f;
		sec_t lastFrameSec = 0.0f;
		size_t frame = 0;
	};
}