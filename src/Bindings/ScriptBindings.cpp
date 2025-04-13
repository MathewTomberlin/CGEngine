#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "../Core/Scripts/Script.h"
#include "../Core/Body/Body.h"
#include "../Core/Behavior/Behavior.h"
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Angle.hpp>
#include "../Core/Engine/Engine.h"

namespace py = pybind11;
void bindScriptTypes(py::module_& m) {
    //CgEngine::ScArgs
    py::class_<CGEngine::ScArgs>(m, "ScArgs")
        .def_readonly("script", &CGEngine::ScArgs::script, py::return_value_policy::reference)
        .def_readonly("caller", &CGEngine::ScArgs::caller, py::return_value_policy::reference)
        .def_readonly("behavior", &CGEngine::ScArgs::behavior, py::return_value_policy::reference)
        .def("__repr__", [](const CGEngine::ScArgs& a) { // Basic representation
        return "<ScArgs script=" + std::string(a.script ? "Script@" + std::to_string(reinterpret_cast<uintptr_t>(a.script)) : "None") +
            " caller=" + std::string(a.caller ? "Body@" + std::to_string(reinterpret_cast<uintptr_t>(a.caller)) : "None") +
            " behavior=" + std::string(a.behavior ? "Behavior@" + std::to_string(reinterpret_cast<uintptr_t>(a.behavior)) : "None") + ">";
            });

    // Define holder type since Script might not be copyable/movable
    // and lifetime is managed by C++ (ScriptDomain/ScriptMap)
    py::class_<CGEngine::Script, std::unique_ptr<CGEngine::Script, py::nodelete>>(m, "Script") // py::nodelete if C++ owns it
        .def("get_id", &CGEngine::Script::getId)
        // Bind Data Accessors (Input/Output/Process Controllers inherited)
        //Float input example
        .def("get_input_data_float", &CGEngine::Script::getInputData<float>, py::arg("key"), "Get float input data")
        .def("set_input_data_float", [](CGEngine::Script& s, const std::string& key, float val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set float input data")

        .def("get_input_data_int", &CGEngine::Script::getInputData<int>, py::arg("key"), "Get int input data")
        .def("set_input_data_int", [](CGEngine::Script& s, const std::string& key, int val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set int input data")

        // Bool Input
        .def("get_input_data_bool", &CGEngine::Script::getInputData<bool>, py::arg("key"), "Get bool input data")
        .def("set_input_data_bool", [](CGEngine::Script& s, const std::string& key, bool val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set bool input data")

        // String Input
        .def("get_input_data_string", &CGEngine::Script::getInputData<std::string>, py::arg("key"), "Get string input data")
        .def("set_input_data_string", [](CGEngine::Script& s, const std::string& key, const std::string& val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set string input data")

        // Vector2f Input (Make sure Vector2f is bound!)
        .def("get_input_data_vector2f", &CGEngine::Script::getInputData<sf::Vector2f>, py::arg("key"), "Get Vector2f input data")
        .def("set_input_data_vector2f", [](CGEngine::Script& s, const std::string& key, const sf::Vector2f& val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set Vector2f input data")

        // --- Repeat for Output Data ---

        // Int Output
        .def("get_output_data_int", &CGEngine::Script::getOutputData<int>, py::arg("key"), "Get int output data")
        .def("set_output_data_int", [](CGEngine::Script& s, const std::string& key, int val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set int output data")

        // Bool Output
        .def("get_output_data_bool", &CGEngine::Script::getOutputData<bool>, py::arg("key"), "Get bool output data")
        .def("set_output_data_bool", [](CGEngine::Script& s, const std::string& key, bool val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set bool output data")

        // Vector2f Output
        .def("get_output_data_vector2f", &CGEngine::Script::getOutputData<sf::Vector2f>, py::arg("key"), "Get Vector2f output data")
        .def("set_output_data_vector2f", [](CGEngine::Script& s, const std::string& key, const sf::Vector2f& val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set Vector2f output data")
        //String output example
        .def("get_output_data_string", &CGEngine::Script::getOutputData<std::string>, py::arg("key"), "Get string output data")
        .def("set_output_data_string", [](CGEngine::Script& s, const std::string& key, const std::string& val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set string output data")
        //TODO: ADD BINDINGS FOR OTHER SUPPORTED TYPES (int, bool, Vector2f, etc.) for input/output/process data
        .def("clear_input_data", static_cast<void (CGEngine::Script::*)()>(&CGEngine::Script::clearInputData)) // Assuming clear exists
        .def("clear_output_data", static_cast<void (CGEngine::Script::*)()>(&CGEngine::Script::clearOutputData))
        //TODO: ... Add bindings for get_input_data_int, set_input_data_bool, etc. ...
        ;

    // Ensure Body is declared - full binding done elsewhere or later
    py::class_<CGEngine::Body, std::unique_ptr<CGEngine::Body, py::nodelete>>(m, "Body") // Holder type again assumes C++ lifetime mgmt
        .def("get_name", &CGEngine::Body::getName) // Add methods scripts might need
        // Add methods needed by my_mover.py or other initial scripts
        .def("move", &CGEngine::Body::translate, py::arg("delta"), py::arg("useWorldRotation") = false, py::arg("useWorldScale") = false, "Move the body locally (relative to parent)")
        // Add getPosition, getRotation, etc. if needed by scripts
        .def("get_position", &CGEngine::Body::getPosition, "Get local position relative to parent")
        .def("get_rotation", &CGEngine::Body::getRotation, "Get local rotation relative to parent")
        .def("get_global_position", &CGEngine::Body::getGlobalPosition, "Get position in world space")
        .def("get_mesh", [](CGEngine::Body& self) -> CGEngine::Mesh* {
            // Body::get<T> returns the entity pointer. We need to cast it.
            // Assuming Mesh inherits from Transformable.
            auto* transformable_ptr = self.get<Transformable*>(); // Get the base pointer
            if (!transformable_ptr) {
                // Handle case where entity is null, maybe throw or return None?
                // For now, returning nullptr which might cause issues in Python if not checked.
                // Consider py::cast(nullptr) or throwing py::type_error
                throw pybind11::type_error("Body entity is null or not accessible.");
                // return nullptr; // Alternative, less safe
            }
            CGEngine::Mesh* mesh_ptr = dynamic_cast<CGEngine::Mesh*>(transformable_ptr);
            if (!mesh_ptr) {
                // Handle case where the entity ISN'T a Mesh.
                throw pybind11::type_error("Body entity is not a Mesh.");
                // return nullptr; // Alternative, less safe
            }
            return mesh_ptr;
        }, py::return_value_policy::reference_internal, "Get the Mesh object associated with this Body (if any).") // Policy is key!
        ;

    // Ensure Behavior is declared
    py::class_<CGEngine::Behavior, std::unique_ptr<CGEngine::Behavior, py::nodelete>>(m, "Behavior")
        .def("get_name", &CGEngine::Behavior::getName)
        // ... Add other needed methods ...
        ;

}