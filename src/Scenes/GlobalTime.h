#pragma once

#include "SFML/Graphics.hpp"
#include "../Types/Types.h"
using namespace sf;

namespace CGEngine {
	class GlobalTime {
	public:
		sec_t getElapsedSec() {
			return runningClock.getElapsedTime().asSeconds();
		}

		sec_t getDeltaSec() const {
			return deltaSec;
		}

		void updateDeltaTime() {
			deltaSec = frameClock.restart().asSeconds();
		}
	private:
		Clock runningClock;
		Clock frameClock;
		sec_t deltaSec = 0.f;
	};
}