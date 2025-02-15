#include "GlobalTime.h"

sec_t GlobalTime::getElapsedSec() {
	return runningClock.getElapsedTime().asSeconds();
}

sec_t GlobalTime::getDeltaSec() const {
	return deltaSec;
}

void GlobalTime::updateDeltaTime() {
	deltaSec = frameClock.restart().asSeconds();
}