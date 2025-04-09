#include <pybind11/embed.h>

namespace CGEngine {
	class InterpreterManager {
		pybind11::scoped_interpreter guard{};
	};
}