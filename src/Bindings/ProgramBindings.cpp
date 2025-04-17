#include <pybind11/pybind11.h>
#include "../Core/Shader/Program.h"
#include "../Core/Types/Types.h"

namespace py = pybind11;

void bindProgram(py::module_& m) {
    // Bind Program. Use nodelete as AssetManager owns it.
    py::class_<CGEngine::Program, CGEngine::IResource, std::unique_ptr<CGEngine::Program, py::nodelete>>(m, "Program")
        .def_property_readonly("id", &CGEngine::Program::getId, "Get the unique ID of the program resource.")
        .def("get_object_id", &CGEngine::Program::getObjectId, "Get the OpenGL object ID.")
        .def("is_valid", &CGEngine::Program::isValid, "Check if the program linked successfully.")
        .def("use", &CGEngine::Program::use, "Use this shader program for subsequent rendering.")
        .def("stop", &CGEngine::Program::stop, "Stop using the current shader program.")
        // Add .def("set_uniform", ...) later if needed directly from Python
        ;
}