#include "Logging.h"

namespace CGEngine {
	LogLevel Logging::logLevel;

	Logging::Logging(LogLevel level) {
		logLevel = level;
	}

	void Logging::operator()(LogLevel level, string caller, string msg, bool newLine) {
		log(level, caller, msg, newLine);
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

	void Logging::log(LogLevel level, string caller, string msg, bool newLine) {
		if (level <= logLevel) {
			string nl = (newLine) ? "\n" : "";
			string callerPrompt = (caller != "") ? caller + ": " : "";
			cout << ":" << callerPrompt << msg << nl;
		}
	}
}