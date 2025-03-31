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
		if (logFPS) {
			if (frame++ % 60 == 0) {
				string FPSStr;
				string FPSLabel = "FPS: ";
				float FPSi = 1.0f / deltaSec;
				string FPS = to_string(FPSi);
				string nl = "\n";
				FPSStr.reserve(FPSLabel.size() + FPS.size()+nl.size());
				FPSStr.append(FPSLabel).append(FPS).append(nl);
				cout << FPSStr;
			}
		}
	}

	sec_t GlobalTime::getSystemTimeMs() {
		auto now = chron::system_clock::now();
		auto time = chron::system_clock::to_time_t(now);
		auto ms = chron::duration_cast<chron::milliseconds>(now.time_since_epoch()) % 1000;
		return ms.count();
	}

	tm GlobalTime::getSystemTimeNow() {
		auto now = chron::system_clock::now();
		auto time = chron::system_clock::to_time_t(now);
		return *localtime(&time);
	}

	string GlobalTime::getSystemmTimeNowString(string delimiter) {
		string systemTimeString;
		tm systemTimeNow = getSystemTimeNow();
		string year = to_string(systemTimeNow.tm_year + 1900);
		string month = to_string(systemTimeNow.tm_mon + 1);
		string day = to_string(systemTimeNow.tm_mday);
		string hour = to_string(systemTimeNow.tm_hour);
		string min = to_string(systemTimeNow.tm_min);
		string sec = to_string(systemTimeNow.tm_sec);
		systemTimeString.reserve(year.size() + delimiter.size() + month.size() + delimiter.size() + day.size() + delimiter.size() + hour.size() + delimiter.size() + min.size() + delimiter.size() + sec.size());
		systemTimeString.append(year).append(delimiter).append(month).append(delimiter).append(day).append(delimiter).append(hour).append(delimiter).append(min).append(delimiter).append(sec);
		return systemTimeString;
	}

	void GlobalTime::startLoggingFPS() {
		logFPS = true;
	}

	void GlobalTime::stopLoggingFPS() {
		logFPS = false;
	}
}