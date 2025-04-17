#include <pybind11/pybind11.h>

#include "../Core/Types/Types.h"
#include "../Core/Engine/EngineSystem.h"

namespace py = pybind11;

void bindCoreTypes(py::module_& m) {
    //LogLevel enum binding
    py::enum_<CGEngine::LogLevel>(m, "LogLevel")
        .value("LogError", CGEngine::LogLevel::LogError)
        .value("LogWarn", CGEngine::LogLevel::LogWarn)
        .value("LogInfo", CGEngine::LogLevel::LogInfo)
        .value("LogDebug", CGEngine::LogLevel::LogDebug)
        .value("LogDebug1", CGEngine::LogLevel::LogDebug1)
        .value("LogDebug2", CGEngine::LogLevel::LogDebug2)
        .export_values();

    //EngineSystem binding
    py::class_<CGEngine::EngineSystem, std::unique_ptr<CGEngine::EngineSystem, py::nodelete>>(m, "EngineSystem")
        // Add methods from EngineSystem.h that Python might need
        .def("get_system_name", &CGEngine::EngineSystem::getSystemName,
            "Get the name automatically assigned to the system.")
        .def("set_system_name", &CGEngine::EngineSystem::setSystemName, py::arg("name"),
            "Set a custom name for the system.")
        .def("get_log_level", &CGEngine::EngineSystem::getLogLevel,
            "Get the current logging level for this system.")
        .def("set_log_level", &CGEngine::EngineSystem::setLogLevel, py::arg("level"),
            "Set the logging level for this system.")
        // .def("init", ...) // init() is protected, typically not called from Python
        ;
    // Use py::nodelete as holder type since derived classes will handle lifetime management
    py::class_<CGEngine::IResource, std::unique_ptr<CGEngine::IResource, py::nodelete>>(m, "IResource")
        .def("is_valid", &CGEngine::IResource::isValid,
            "Check if the resource is valid.")
        .def("get_id", &CGEngine::IResource::getId,
            "Get the unique ID of the resource, or None if not set.")
        // No constructor needed for an interface/abstract base usually
        ;

    // Bind other core types/structs from Types.h here if needed later
    // e.g., py::class_<CGEngine::SurfaceParameters>(m, "SurfaceParameters")...
}