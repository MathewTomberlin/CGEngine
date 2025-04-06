#pragma once

#include <optional>

namespace CGEngine {
	class Timer {
	public:
		Timer(string n = "") {
			name = n;
			eventId = 0U;
		}
		optional<size_t> id = nullopt;
		size_t eventId = 0U;
		string name = "";
	};
}