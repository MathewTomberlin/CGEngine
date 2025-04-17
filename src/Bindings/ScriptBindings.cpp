#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "../Core/Scripts/Script.h"
#include "../Core/Body/Body.h"
#include "../Core/Behavior/Behavior.h"
#include "../Core/Timers/TimerMap.h"
#include "../Core/Engine/Engine.h"
#include "../Core/Mesh/Mesh.h"
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Angle.hpp>

namespace py = pybind11;
using ScriptEventHandler = std::variant<CGEngine::ScriptEvent, pybind11::object>;
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
        
        // Float input
        .def("get_input_data_float", &CGEngine::Script::getInputData<float>, py::arg("key"), "Get float input data")
        .def("set_input_data_float", [](CGEngine::Script& s, const std::string& key, float val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set float input data")
        // Int input
        .def("get_input_data_int", &CGEngine::Script::getInputData<int>, py::arg("key"), "Get int input data")
        .def("set_input_data_int", [](CGEngine::Script& s, const std::string& key, int val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set int input data")

        // Vector3f Input
        .def("get_input_data_vector3f", &CGEngine::Script::getInputData<sf::Vector3f>, py::arg("key"), "Get Vector3f input data")
        .def("set_input_data_vector3f", [](CGEngine::Script& s, const std::string& key, const sf::Vector3f& val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set Vector3f input data")

        // Color Input
        .def("get_input_data_color", &CGEngine::Script::getInputData<sf::Color>, py::arg("key"), "Get Color input data")
        .def("set_input_data_color", [](CGEngine::Script& s, const std::string& key, const sf::Color& val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set Color input data")

        // Angle Input
        .def("get_input_data_angle", &CGEngine::Script::getInputData<sf::Angle>, py::arg("key"), "Get Angle input data")
        .def("set_input_data_angle", [](CGEngine::Script& s, const std::string& key, const sf::Angle& val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set Angle input data")

        // Bool Input
        .def("get_input_data_bool", &CGEngine::Script::getInputData<bool>, py::arg("key"), "Get bool input data")
        .def("set_input_data_bool", [](CGEngine::Script& s, const std::string& key, bool val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set bool input data")

        // String Input
        .def("get_input_data_string", &CGEngine::Script::getInputData<std::string>, py::arg("key"), "Get string input data")
        .def("set_input_data_string", [](CGEngine::Script& s, const std::string& key, const std::string& val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set string input data")

        // Vector2f Input (Make sure Vector2f is bound!)
        .def("get_input_data_vector2f", &CGEngine::Script::getInputData<sf::Vector2f>, py::arg("key"), "Get Vector2f input data")
        .def("set_input_data_vector2f", [](CGEngine::Script& s, const std::string& key, const sf::Vector2f& val) { s.setInputData(key, val); }, py::arg("key"), py::arg("value"), "Set Vector2f input data")

        // Int Output
        .def("get_output_data_int", &CGEngine::Script::getOutputData<int>, py::arg("key"), "Get int output data")
        .def("set_output_data_int", [](CGEngine::Script& s, const std::string& key, int val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set int output data")

        // Bool Output
        .def("get_output_data_bool", &CGEngine::Script::getOutputData<bool>, py::arg("key"), "Get bool output data")
        .def("set_output_data_bool", [](CGEngine::Script& s, const std::string& key, bool val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set bool output data")

        // Vector2f Output
        .def("get_output_data_vector2f", &CGEngine::Script::getOutputData<sf::Vector2f>, py::arg("key"), "Get Vector2f output data")
        .def("set_output_data_vector2f", [](CGEngine::Script& s, const std::string& key, const sf::Vector2f& val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set Vector2f output data")

        //String output
        .def("get_output_data_string", &CGEngine::Script::getOutputData<std::string>, py::arg("key"), "Get string output data")
        .def("set_output_data_string", [](CGEngine::Script& s, const std::string& key, const std::string& val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set string output data")

        // Vector3f Output
        .def("get_output_data_vector3f", &CGEngine::Script::getOutputData<sf::Vector3f>, py::arg("key"), "Get Vector3f output data")
        .def("set_output_data_vector3f", [](CGEngine::Script& s, const std::string& key, const sf::Vector3f& val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set Vector3f output data")

        // Color Output
        .def("get_output_data_color", &CGEngine::Script::getOutputData<sf::Color>, py::arg("key"), "Get Color output data")
        .def("set_output_data_color", [](CGEngine::Script& s, const std::string& key, const sf::Color& val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set Color output data")

        // Angle Output
        .def("get_output_data_angle", &CGEngine::Script::getOutputData<sf::Angle>, py::arg("key"), "Get Angle output data")
        .def("set_output_data_angle", [](CGEngine::Script& s, const std::string& key, const sf::Angle& val) { s.setOutputData(key, val); }, py::arg("key"), py::arg("value"), "Set Angle output data")

        // Clear Input/Output
        .def("clear_input_data", static_cast<void (CGEngine::Script::*)()>(&CGEngine::Script::clearInputData))
        .def("clear_output_data", static_cast<void (CGEngine::Script::*)()>(&CGEngine::Script::clearOutputData))
        ;

    // Ensure Body is declared - full binding done elsewhere or later
    py::class_<CGEngine::Body, std::unique_ptr<CGEngine::Body, py::nodelete>>(m, "Body") // Holder type again assumes C++ lifetime mgmt
        .def("get_name", &CGEngine::Body::getName) // Add methods scripts might need
        // Add methods needed by my_mover.py or other initial scripts
        .def("translate", &CGEngine::Body::translate, py::arg("delta"), py::arg("useWorldRotation") = false, py::arg("useWorldScale") = false, "Move the body locally (relative to parent)")
        .def("move", py::overload_cast<sf::Vector2f>(&CGEngine::Body::move), py::arg("offset"), "Move the body by an offset (relative to its current orientation)")
        .def("move", [](CGEngine::Body& self, float x, float y) {self.move({ x,y }); }, py::arg("x"), py::arg("x"), "Move the body by an offset (relative to its current orientation)")
        // Add getPosition, getRotation, etc. if needed by scripts
        .def("get_position", &CGEngine::Body::getPosition, "Get local position relative to parent")
        .def("get_rotation", &CGEngine::Body::getRotation, "Get local rotation relative to parent")
        .def("get_global_position", &CGEngine::Body::getGlobalPosition, "Get position in world space")

        // Transform Setters (from sf::Transformable inheritance)
        // Bind setPosition taking Vector2f
        .def("set_position", py::overload_cast<sf::Vector2f>(&sf::Transformable::setPosition), py::arg("position"), "Set the local position relative to the parent")
        // Bind setPosition taking x, y floats (for convenience)
        .def("set_position", [](CGEngine::Body& self, float x, float y) {self.setPosition({ x,y }); }, py::arg("x"), py::arg("y"), "Set the local position relative to the parent using x, y coordinates")

        // Bind setRotation taking Angle
        .def("set_rotation", py::overload_cast<sf::Angle>(&sf::Transformable::setRotation), py::arg("angle"), "Set the local orientation")
        // Bind setRotation taking float degrees (for convenience)
        .def("set_rotation", [](CGEngine::Body& self, float degrees) { self.setRotation(sf::degrees(degrees)); }, py::arg("degrees"), "Set the local orientation using degrees")

        // Bind setScale taking Vector2f
        .def("set_scale", py::overload_cast<sf::Vector2f>(&sf::Transformable::setScale), py::arg("scale"), "Set the local scale factors")
        // Bind setScale taking x, y floats (for convenience)
        .def("set_scale", [](CGEngine::Body& self, float x, float y) {self.setScale({ x,y }); }, py::arg("x"), py::arg("x"), "Set the local scale factors using x, y components")

        // State Getters/Setters
        .def("get_rendering_enabled", &CGEngine::Body::getRenderingEnabled, "Check if the body rendering is enabled")
        .def("set_rendering_enabled", &CGEngine::Body::setRenderingEnabled, py::arg("enabled"), "Enable or disable rendering for this body")
        .def("get_intersect_enabled", &CGEngine::Body::getIntersectEnabled, "Check if intersection testing is enabled for this body")
        .def("set_intersect_enabled", &CGEngine::Body::setIntersectEnabled, py::arg("enabled"), "Enable or disable intersection testing for this body")
        // Add bounds rendering getters/setters if needed
        .def("get_bounds_rendering_enabled", &CGEngine::Body::getBoundsRenderingEnabled, "Check if bounds rendering is enabled")
        .def("set_bounds_rendering_enabled", &CGEngine::Body::setBoundsRenderingEnabled, py::arg("enabled"), "Enable or disable bounds rendering")

        // Child Count
        .def("get_child_count", &CGEngine::Body::getChildCount, "Get the number of direct children attached to this body")

        // Hierarchy
        .def("get_parent", &CGEngine::Body::getParent, py::return_value_policy::reference,
            "Get the parent Body object, or None if it's the root or detached.")
        // Note: getChildren cannot be bound as there's no public C++ method returning them.

        // Behavior Interaction
        .def("get_behavior", &CGEngine::Body::getBehavior, py::arg("behavior_id"), py::return_value_policy::reference,
            "Get a specific Behavior attached to this Body by its ID.")
        // Note: createBehavior is deferred due to template complexity.

        // Timer Interaction
        .def("set_timer", &CGEngine::Body::setTimer, py::arg("duration"), py::arg("on_complete_script"), py::arg("loop_count") = 0, py::arg("timer_display_name") = "",
            "Set a timer that calls the script when complete. Returns timer ID or None.",
            py::return_value_policy::automatic)
        .def("cancel_timer", py::overload_cast<size_t>(&CGEngine::Body::cancelTimer), py::arg("timer_id"), "Cancel the timer with the specified ID.")

        // Entity Access (Specific Instantiations of get<T>)
        .def("get_shape", [](CGEngine::Body& self) -> sf::Shape* {
        // Need error handling if dynamic_cast fails
        try { return self.get<sf::Shape*>(); }
        catch (...) { return nullptr; } }, py::return_value_policy::reference_internal, //Entity is unique_ptr in Body, owned by C++
            "Get the underlying entity as a sf::Shape pointer (if applicable).")

        .def("get_text", [](CGEngine::Body& self) -> sf::Text* {
        try { return self.get<sf::Text*>(); }
        catch (...) { return nullptr; } }, py::return_value_policy::reference_internal, //Entity is unique_ptr in Body, owned by C++
            "Get the underlying entity as a sf::Text pointer (if applicable).")

        .def("get_sprite", [](CGEngine::Body& self) -> sf::Sprite* {
        try { return self.get<sf::Sprite*>(); }
        catch (...) { return nullptr; } }, py::return_value_policy::reference_internal, //Entity is unique_ptr in Body, owned by C++
            "Get the underlying entity as a sf::Sprite pointer (if applicable).")

        .def("get_mesh", [](CGEngine::Body& self) -> CGEngine::Mesh* {
            // Body::get<T> returns the entity pointer. We need to cast it.
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

        //.def("addKeyHoldScript", [](CGEngine::Body& self, const std::string& module_name, int key) {
        //    py::module_ m_engine = py::module_::import("cge_engine");
        //    auto py_interpreter = m_engine.attr("py_interpreter").cast<py::object>();
        //    if (py_interpreter.is_none()) {
        //        throw std::runtime_error("Global PyInterpreter not found in module.");
        //    }
        //
        //    py::object py_script_obj;
        //    try {
        //        py_script_obj = py_interpreter.attr("createScript")(module_name);
        //    }
        //    catch (const py::error_already_set& e) {
        //        throw std::runtime_error("Error creating Python script from module '" + module_name + "': " + e.what());
        //    }
        //
        //    if (py_script_obj.is_none()) {
        //        throw std::runtime_error("Failed to create Python script object from module '" + module_name + "'.");
        //    }
        //
        //    CGEngine::Script* script_instance = new CGEngine::Script(py_script_obj);
        //    self.addKeyHoldScript(script_instance, static_cast<sf::Keyboard::Scan>(key));
        //}, py::arg("module_name"), py::arg("key"))
        //.def("addMousePressScript", [](CGEngine::Body& self, const std::string& module_name, sf::Mouse::Button button, py::object behavior_id) {
        //    py::module_ m_engine = py::module_::import("cge_engine");
        //    auto py_interpreter = m_engine.attr("py_interpreter").cast<py::object>();
        //    if (py_interpreter.is_none()) {
        //        throw std::runtime_error("Global PyInterpreter not found in module.");
        //    }
        //
        //    py::object py_script_evt;
        //    try {
        //        py_script_evt = py_interpreter.attr("createScript")(module_name);
        //    }
        //    catch (const py::error_already_set& e) {
        //        throw std::runtime_error("Error creating Python script from module '" + module_name + "': " + e.what());
        //    }
        //
        //    if (py_script_evt.is_none()) {
        //        throw std::runtime_error("Failed to create Python script object from module '" + module_name + "'.");
        //    }
        //
        //    CGEngine::Body::ScriptEventHandler scriptEventHandler;
        //    scriptEventHandler = py_script_evt;
        //
        //    std::optional<CGEngine::id_t> behaviorIdOpt = std::nullopt;
        //    if (!behavior_id.is_none()) {
        //        behaviorIdOpt = behavior_id.cast<CGEngine::id_t>();
        //    }
        //
        //    return self.addMousePressScript(scriptEventHandler, button, behaviorIdOpt);
        //}, py::arg("module_name"), py::arg("button") = sf::Mouse::Button::Left, py::arg("behavior_id") = py::none())
        //.def("addMouseReleaseScript", [](CGEngine::Body& self, const std::string& module_name, sf::Mouse::Button button, py::object behavior_id) {
        //    py::module_ m_engine = py::module_::import("cge_engine");
        //    auto py_interpreter = m_engine.attr("py_interpreter").cast<py::object>();
        //    if (py_interpreter.is_none()) {
        //        throw std::runtime_error("Global PyInterpreter not found in module.");
        //    }
        //
        //    py::object py_script_evt;
        //    try {
        //        py_script_evt = py_interpreter.attr("createScript")(module_name);
        //    }
        //    catch (const py::error_already_set& e) {
        //        throw std::runtime_error("Error creating Python script from module '" + module_name + "': " + e.what());
        //    }
        //
        //    if (py_script_evt.is_none()) {
        //        throw std::runtime_error("Failed to create Python script object from module '" + module_name + "'.");
        //    }
        //
        //    CGEngine::Body::ScriptEventHandler scriptEventHandler;
        //    scriptEventHandler = py_script_evt;
        //
        //    std::optional<CGEngine::id_t> behaviorIdOpt = std::nullopt;
        //    if (!behavior_id.is_none()) {
        //        behaviorIdOpt = behavior_id.cast<CGEngine::id_t>();
        //    }
        //
        //    return self.addMouseReleaseScript(scriptEventHandler, button, behaviorIdOpt);
        //}, py::arg("module_name"), py::arg("button") = sf::Mouse::Button::Left, py::arg("behavior_id") = py::none())
        //.def("addKeyPressScript", [](CGEngine::Body& self, const std::string& module_name, sf::Keyboard::Scan key, py::object behavior_id) {
        //    py::module_ m_engine = py::module_::import("cge_engine");
        //    auto py_interpreter = m_engine.attr("py_interpreter").cast<py::object>();
        //    if (py_interpreter.is_none()) {
        //        throw std::runtime_error("Global PyInterpreter not found in module.");
        //    }
        //
        //    py::object py_script_evt;
        //    try {
        //        py_script_evt = py_interpreter.attr("createScript")(module_name);
        //    }
        //    catch (const py::error_already_set& e) {
        //        throw std::runtime_error("Error creating Python script from module '" + module_name + "': " + e.what());
        //    }
        //
        //    if (py_script_evt.is_none()) {
        //        throw std::runtime_error("Failed to create Python script object from module '" + module_name + "'.");
        //    }
        //
        //    CGEngine::Body::ScriptEventHandler scriptEventHandler;
        //    scriptEventHandler = py_script_evt;
        //
        //    std::optional<CGEngine::id_t> behaviorIdOpt = std::nullopt;
        //    if (!behavior_id.is_none()) {
        //        behaviorIdOpt = behavior_id.cast<CGEngine::id_t>();
        //    }
        //
        //    return self.addKeyPressScript(scriptEventHandler, key, behaviorIdOpt);
        //}, py::arg("module_name"), py::arg("button") = sf::Keyboard::Scan::Space, py::arg("behavior_id") = py::none())
        //.def("addKeyReleaseScript", [](CGEngine::Body& self, const std::string& module_name, sf::Keyboard::Scan key, py::object behavior_id) {
        //    py::module_ m_engine = py::module_::import("cge_engine");
        //    auto py_interpreter = m_engine.attr("py_interpreter").cast<py::object>();
        //    if (py_interpreter.is_none()) {
        //        throw std::runtime_error("Global PyInterpreter not found in module.");
        //    }
        //
        //    py::object py_script_evt;
        //    try {
        //        py_script_evt = py_interpreter.attr("createScript")(module_name);
        //    }
        //    catch (const py::error_already_set& e) {
        //        throw std::runtime_error("Error creating Python script from module '" + module_name + "': " + e.what());
        //    }
        //
        //    if (py_script_evt.is_none()) {
        //        throw std::runtime_error("Failed to create Python script object from module '" + module_name + "'.");
        //    }
        //
        //    CGEngine::Body::ScriptEventHandler scriptEventHandler;
        //    scriptEventHandler = py_script_evt;
        //
        //    std::optional<CGEngine::id_t> behaviorIdOpt = std::nullopt;
        //    if (!behavior_id.is_none()) {
        //        behaviorIdOpt = behavior_id.cast<CGEngine::id_t>();
        //    }
        //
        //    return self.addKeyReleaseScript(scriptEventHandler, key, behaviorIdOpt);
        //}, py::arg("module_name"), py::arg("button") = sf::Keyboard::Scan::Space, py::arg("behavior_id") = py::none())
        ;
}