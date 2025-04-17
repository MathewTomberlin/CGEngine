#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../Core/Engine/Engine.h"
#include "../Core/Behavior/Behavior.h"
#include "../Core/Body/Body.h"
#include "../Core/Scripts/Script.h"
#include "../Core/Types/DataMap.h"     // For callDomainWithData - maybe just use dict?

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Angle.hpp>

namespace py = pybind11;

// Helper function to convert a Python object to std::any
std::any convert_pyobject_to_any(const py::object& obj) {
    if (py::isinstance<py::bool_>(obj)) {
        return obj.cast<bool>();
    }
    else if (py::isinstance<py::int_>(obj)) {
        return obj.cast<int>();
    }
    else if (py::isinstance<py::float_>(obj)) {
        return obj.cast<float>();
    }
    else if (py::isinstance<py::str>(obj)) {
        return obj.cast<std::string>();
    }
    else if (py::isinstance<py::list>(obj)) {
        // Handle lists - this is a simplification and might need more robust handling
        // depending on the list contents (e.g., lists of lists, etc.)
        return obj.cast<std::vector<py::object>>();
    }
    else if (py::isinstance<py::tuple>(obj)) {
        // Handle tuples - similar to lists
        return obj.cast<std::tuple<py::object>>();
    }
    else if (py::isinstance<py::dict>(obj)) {
        // Handle Dictionaries
        return obj.cast<std::map<std::string, py::object>>();
    }
    else if (py::isinstance<py::array>(obj)) {
        return obj.cast<py::array>();
    }
    else {
        throw std::runtime_error("Unsupported type for conversion to std::any");
    }
}

// Forward declaration if Body class is defined elsewhere
// void bindBody(py::module_&);
void bindBehavior(py::module_& m) {
    py::enum_<CGEngine::StaticScriptDomain>(m, "StaticScriptDomain")
        .value("StartDomain", CGEngine::StaticScriptDomain::StartDomain)
        .value("UpdateDomain", CGEngine::StaticScriptDomain::UpdateDomain)
        .value("DeleteDomain", CGEngine::StaticScriptDomain::DeleteDomain)
        .export_values();

    // Define the Behavior class binding
    py::class_<CGEngine::Behavior, std::unique_ptr<CGEngine::Behavior, py::nodelete>>(m, "Behavior")
        .def("get_id", &CGEngine::Behavior::getId, "Get the unique ID of the Behavior within its owner Body.")
        .def("get_owner", &CGEngine::Behavior::getOwner, py::return_value_policy::reference, "Get the owning Body object.")
        .def("get_name", &CGEngine::Behavior::getName, "Get the display name of the Behavior.")

        // --- Data Controller Methods ---
        // Input Data
        .def("get_input_data_float", &CGEngine::Behavior::getInputData<float>, py::arg("key"))
        .def("set_input_data_float", [](CGEngine::Behavior& b, const std::string& key, float val) { b.setInputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_input_data_int", &CGEngine::Behavior::getInputData<int>, py::arg("key"))
        .def("set_input_data_int", [](CGEngine::Behavior& b, const std::string& key, int val) { b.setInputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_input_data_bool", &CGEngine::Behavior::getInputData<bool>, py::arg("key"))
        .def("set_input_data_bool", [](CGEngine::Behavior& b, const std::string& key, bool val) { b.setInputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_input_data_string", &CGEngine::Behavior::getInputData<std::string>, py::arg("key"))
        .def("set_input_data_string", [](CGEngine::Behavior& b, const std::string& key, const std::string& val) { b.setInputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_input_data_vector2f", &CGEngine::Behavior::getInputData<sf::Vector2f>, py::arg("key"))
        .def("set_input_data_vector2f", [](CGEngine::Behavior& b, const std::string& key, const sf::Vector2f& val) { b.setInputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_input_data_vector3f", &CGEngine::Behavior::getInputData<sf::Vector3f>, py::arg("key"))
        .def("set_input_data_vector3f", [](CGEngine::Behavior& b, const std::string& key, const sf::Vector3f& val) { b.setInputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_input_data_color", &CGEngine::Behavior::getInputData<sf::Color>, py::arg("key"))
        .def("set_input_data_color", [](CGEngine::Behavior& b, const std::string& key, const sf::Color& val) { b.setInputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_input_data_angle", &CGEngine::Behavior::getInputData<sf::Angle>, py::arg("key"))
        .def("set_input_data_angle", [](CGEngine::Behavior& b, const std::string& key, const sf::Angle& val) { b.setInputData(key, val); }, py::arg("key"), py::arg("value"))
        // Add more types as needed...

        // Output Data (Expose specific types)
        .def("get_output_data_float", &CGEngine::Behavior::getOutputData<float>, py::arg("key"))
        .def("set_output_data_float", [](CGEngine::Behavior& b, const std::string& key, float val) { b.setOutputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_output_data_int", &CGEngine::Behavior::getOutputData<int>, py::arg("key"))
        .def("set_output_data_int", [](CGEngine::Behavior& b, const std::string& key, int val) { b.setOutputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_output_data_bool", &CGEngine::Behavior::getOutputData<bool>, py::arg("key"))
        .def("set_output_data_bool", [](CGEngine::Behavior& b, const std::string& key, bool val) { b.setOutputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_output_data_string", &CGEngine::Behavior::getOutputData<std::string>, py::arg("key"))
        .def("set_output_data_string", [](CGEngine::Behavior& b, const std::string& key, const std::string& val) { b.setOutputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_output_data_vector2f", &CGEngine::Behavior::getOutputData<sf::Vector2f>, py::arg("key"))
        .def("set_output_data_vector2f", [](CGEngine::Behavior& b, const std::string& key, const sf::Vector2f& val) { b.setOutputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_output_data_vector3f", &CGEngine::Behavior::getOutputData<sf::Vector3f>, py::arg("key"))
        .def("set_output_data_vector3f", [](CGEngine::Behavior& b, const std::string& key, const sf::Vector3f& val) { b.setOutputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_output_data_color", &CGEngine::Behavior::getOutputData<sf::Color>, py::arg("key"))
        .def("set_output_data_color", [](CGEngine::Behavior& b, const std::string& key, const sf::Color& val) { b.setOutputData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_output_data_angle", &CGEngine::Behavior::getOutputData<sf::Angle>, py::arg("key"))
        .def("set_output_data_angle", [](CGEngine::Behavior& b, const std::string& key, const sf::Angle& val) { b.setOutputData(key, val); }, py::arg("key"), py::arg("value"))


        // Process Data (Expose specific types)
        .def("get_process_data_float", &CGEngine::Behavior::getProcessData<float>, py::arg("key"))
        .def("set_process_data_float", [](CGEngine::Behavior& b, const std::string& key, float val) { b.setProcessData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_process_data_int", &CGEngine::Behavior::getProcessData<int>, py::arg("key"))
        .def("set_process_data_int", [](CGEngine::Behavior& b, const std::string& key, int val) { b.setProcessData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_process_data_bool", &CGEngine::Behavior::getProcessData<bool>, py::arg("key"))
        .def("set_process_data_bool", [](CGEngine::Behavior& b, const std::string& key, bool val) { b.setProcessData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_process_data_string", &CGEngine::Behavior::getProcessData<std::string>, py::arg("key"))
        .def("set_process_data_string", [](CGEngine::Behavior& b, const std::string& key, const std::string& val) { b.setProcessData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_process_data_vector2f", &CGEngine::Behavior::getProcessData<sf::Vector2f>, py::arg("key"))
        .def("set_process_data_vector2f", [](CGEngine::Behavior& b, const std::string& key, const sf::Vector2f& val) { b.setProcessData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_process_data_vector3f", &CGEngine::Behavior::getProcessData<sf::Vector3f>, py::arg("key"))
        .def("set_process_data_vector3f", [](CGEngine::Behavior& b, const std::string& key, const sf::Vector3f& val) { b.setProcessData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_process_data_color", &CGEngine::Behavior::getProcessData<sf::Color>, py::arg("key"))
        .def("set_process_data_color", [](CGEngine::Behavior& b, const std::string& key, const sf::Color& val) { b.setProcessData(key, val); }, py::arg("key"), py::arg("value"))
        .def("get_process_data_angle", &CGEngine::Behavior::getProcessData<sf::Angle>, py::arg("key"))
        .def("set_process_data_angle", [](CGEngine::Behavior& b, const std::string& key, const sf::Angle& val) { b.setProcessData(key, val); }, py::arg("key"), py::arg("value"))


        // --- ScriptMap Methods ---
        .def("add_script", &CGEngine::Behavior::addScript, py::arg("domain"), py::arg("script"), "Add a script to a specific domain.")
        // removeScript has C++ overloads (string, id_t, bool) and (string, Script*, bool)
        // Binding the ID version is usually sufficient and safer for Python.
        .def("remove_script", py::overload_cast<std::string, CGEngine::id_t, bool>(&CGEngine::Behavior::removeScript),
            py::arg("domain"), py::arg("script_id"), py::arg("should_delete_domain_if_empty") = false,
            "Remove a script by its ID from a specific domain.")

        // callDomain has overloads: (string) and (StaticScriptDomain)
        .def("call_domain", py::overload_cast<std::string>(&CGEngine::Behavior::callDomain),
            py::arg("domain"), "Call all scripts in the specified domain.")
        .def("call_static_domain", py::overload_cast<CGEngine::StaticScriptDomain>(&CGEngine::Behavior::callStaticDomain),
            py::arg("domain_id"), "Call all scripts in a static domain (Start, Update, Delete).")

        .def("call_domain_with_data", [](CGEngine::Behavior& behavior, const std::string& domain, const py::dict& data) {
        CGEngine::DataMap data_map;
        try {
            for (auto it : data) {
                std::string key = it.first.cast<std::string>();
                py::object value = py::cast<py::object>(it.second);
                data_map.setData(key, convert_pyobject_to_any(value));
            }
            behavior.callDomainWithData(domain, data_map);
        }
        catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_TypeError, e.what());
            throw py::error_already_set();
        }
        catch (const py::error_already_set& e) {
            throw;
        }
        catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
            throw py::error_already_set();
        }
        catch (...) {
            PyErr_SetString(PyExc_RuntimeError, "An unknown error occurred during data conversion.");
            throw py::error_already_set();
        }
            }, py::arg("domain"), py::arg("data"), "Call all scripts in the specified domain, passing the provided data.");

    // addScriptEventsByDomain might be complex if ScriptEvent isn't easily passable from Python.
    // It might be easier for Python scripts to just use add_script multiple times.
    // .def("add_script_events_by_domain", ...) // Deferred
    ;
}