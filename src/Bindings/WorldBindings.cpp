#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include "../Core/Engine/Engine.h"
#include "../Core/World/World.h"
#include "../Core/Body/Body.h"
#include "../Core/Scripts/Script.h"
#include "../Core/Types/DataMap.h"

namespace py = pybind11;

void bindWorld(py::module_& m) {
    py::class_<CGEngine::World, CGEngine::EngineSystem, std::unique_ptr<CGEngine::World, py::nodelete>>(m, "World")
        .def(py::init<>())
        .def("run_world", &CGEngine::World::runWorld, "Starts the main world loop.")
        .def("start_world", &CGEngine::World::startWorld, "Initializes the world.")
        .def("end_world", &CGEngine::World::endWorld, "Cleans up and ends the world.")
        .def("call_scripts", &CGEngine::World::callScripts, py::arg("script_domain"), py::arg("body") = py::none(), "Calls scripts in a domain for a specific body (or the root).")
        .def("call_static_scripts", &CGEngine::World::callStaticScripts, py::arg("script_domain_id"), py::arg("body") = py::none(), "Calls static scripts (Start, Update, Delete) for a body (or the root).")
        .def("add_default_exit_actuator", &CGEngine::World::addDefaultExitActuator, "Adds a default exit actuator (e.g., Escape key).")
        .def("z_raycast", &CGEngine::World::zRayCast, py::arg("world_position"), py::arg("start_z") = py::none(), py::arg("distance") = -1, py::arg("backward") = false, py::arg("linecast") = false, "Performs a Z-axis raycast.")
        .def("raycast", &CGEngine::World::raycast, py::arg("world_position"), py::arg("cast_direction"), py::arg("z_index") = 0, py::arg("distance") = -1.f, py::arg("linecast") = false, "Performs a 2D raycast.")
        .def("initialize_console", &CGEngine::World::initializeConsole, "Initializes the in-game console.")
        .def_readwrite("console_input_enabled", &CGEngine::World::consoleInputEnabled, "Enables or disables console input.")
        .def("set_bounds_rendering_enabled", &CGEngine::World::setBoundsRenderingEnabled, py::arg("enabled"), "Enables or disables bounds rendering for all bodies.")
        .def("get_bounds_rendering_enabled", &CGEngine::World::getBoundsRenderingEnabled, "Gets the current state of bounds rendering.")
        .def("set_bounds_color", &CGEngine::World::setBoundsColor, py::arg("color"), "Sets the color of the bounds.")
        .def("set_bounds_thickness", &CGEngine::World::setBoundsThickness, py::arg("thickness"), "Sets the thickness of the bounds.")
        .def("add_scene", &CGEngine::World::addScene, py::arg("scene_name"), py::arg("scene"), "Adds a scene to the world.")
        .def("load_scene", &CGEngine::World::loadScene, py::arg("scene_name"), "Loads a scene.")
        .def("load_scene_with_input", &CGEngine::World::loadSceneWithInput, py::arg("scene_name"), py::arg("input"), "Loads a scene with input data.")
        .def("get_scene_input", &CGEngine::World::getSceneInput, py::arg("scene_name"), "Gets the input data for a scene.")
        .def("get_scene_output", &CGEngine::World::getSceneOutput, py::arg("scene_name"), "Gets the output data for a scene.")
        .def("get_scene_process", &CGEngine::World::getSceneProcess, py::arg("scene_name"), "Gets the process data for a scene.")
        .def("add_world_script", &CGEngine::World::addWorldScript, py::arg("domain"), py::arg("script"), "Adds a script to the world (root body).")
        .def("get_root", &CGEngine::World::getRoot, py::return_value_policy::reference, "Gets the root body of the world.");
}