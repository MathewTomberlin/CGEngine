#include "GlobalTime.h"
#include <iostream>

using namespace std;

namespace CGEngine {
	sec_t GlobalTime::getElapsedSec() {
		return runningClock.getElapsedTime().asSeconds();
	}

	sec_t GlobalTime::getLastFrameSec() {
		return getLastFrameSec();
	}

	sec_t GlobalTime::getDeltaSec() const {
		return deltaSec;
	}

	void GlobalTime::updateDeltaTime() {
		lastFrameSec = currentFrameSec;
		currentFrameSec = getElapsedSec();
		deltaSec = frameClock.restart().asSeconds();
		//if (frame++ % 60 == 0) {
		//	cout << "FPS: " << (1.0f / deltaSec) << "\n";
		//}
	}
}