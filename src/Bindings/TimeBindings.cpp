#include <pybind11/pybind11.h>
#include "../Core/Engine/Engine.h"
#include "../Core/Time/GlobalTime.h"

namespace py = pybind11;

void bindTimeTypes(py::module_& m) {
    m.def("get_delta_time", []() -> CGEngine::sec_t {
            return CGEngine::time.getDeltaSec();
        }, "Get the time elapsed since the last frame in seconds (delta time).");

    m.def("get_elapsed_time", []() -> CGEngine::sec_t {
            return CGEngine::time.getElapsedSec();
        }, "Get the total time elapsed since the engine started in seconds.");

    // m.def("get_system_time_ms", []() { return CGEngine::time.getSystemTimeMs(); }, "Get system time milliseconds.");
}