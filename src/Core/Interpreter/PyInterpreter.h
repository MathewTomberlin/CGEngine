#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

inline std::filesystem::path getExecutableDirectory() {
    return std::filesystem::current_path();
}

namespace CGEngine {
    namespace py = pybind11;

	class PyInterpreter {
    public:
        PyInterpreter() { 
            try {
                std::filesystem::path exeDir = getExecutableDirectory();
                std::filesystem::path bindingsDir = exeDir;
                std::filesystem::path userScriptsDir = exeDir / "scripts"; // User scripts location (/bin/scripts)
                std::filesystem::path enginePythonDir = exeDir / "cg_engine_python" / "Scripts"; //Engine base Python class

                py::module_ sys = py::module_::import("sys");
                py::list sysPath = sys.attr("path");

                // Check if paths already exist to avoid duplicates
                auto path_exists = [&](const std::filesystem::path& p) {
                    for (const auto& item : sysPath) {
                        if (py::cast<std::string>(item) == p.string()) {
                            return true;
                        }
                    }
                    return false;
                };

                //Add bindings dir
                if (!path_exists(bindingsDir)) {
                    sysPath.append(bindingsDir.string());
                }
                
                //Add user Python scripts dir
                if (!path_exists(userScriptsDir)) {
                    sysPath.append(userScriptsDir.string());
                }

                //Add engine Python scripts dir
                if (!path_exists(enginePythonDir)) {
                    sysPath.append(enginePythonDir.string());
                }
            }
            catch (const py::error_already_set& e) {
                throw;
			} catch (const std::exception& e) {
                std::cerr << "[PyInterpreter] Fatal C++ Error during initialization: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[PyInterpreter] Fatal Unknown Error during initialization." << std::endl;
                throw;
            }
        }
        ~PyInterpreter() = default;

        /**
         * @brief Executes a Python script file.
         *
         * @param scriptPath The absolute path to the Python script file,
         * or a path relative to one of the directories in sys.path
         * (like the configured 'resources/scripts' directory).
         * @return True if the script executed without raising an exception, False otherwise.
         */
        bool runScript(const std::string& scriptPath) {
            // Acquire the Python Global Interpreter Lock
            py::gil_scoped_acquire acquire;

            try {
                // Use py::eval_file to execute the script.
                // It executes the file in the __main__ module's context.
                py::eval_file(scriptPath);

                // Check if a Python exception occurred *during* execution but was handled within Python
                if (PyErr_Occurred()) {
                    py::error_already_set error; // Fetch the error
                    std::cerr << "[PyInterpreter] Python handled error running PyScript '" << scriptPath << "': " << error.what() << std::endl;
                    PyErr_Clear(); // Clear the error state
                    return false;
                }
                return true;

            } catch (const py::error_already_set& e) {
                // Catch exceptions thrown by py::eval_file (syntax errors, runtime exceptions)
                std::cerr << "[PyInterpreter] Python error running PyScript '" << scriptPath << e.what() << std::endl;
                // PyErr_Clear() is implicitly handled by error_already_set destructor
                return false;
            } catch (const std::exception& e) {
                // Catch C++ exceptions during the process
                std::cerr << "[PyInterpreter] C++ error running PyScript'" << scriptPath << "': " << e.what() << std::endl;
                return false;
            }
            // GIL is automatically released when 'acquire' goes out of scope
        }

        /// <summary>
        /// Where script path is expected to equal scriptDirectory/moduleName.py, try to import the module at the script path, and
        /// if successful, construct a new Script using the imported PyScript
        /// </summary>
        /// <param name="moduleName">The name of the imported Python script module. Should match the filename without extension</param>
        /// <returns>The constructed Script</returns>
        Script* createScript(const std::string& moduleName) {
            // Acquire the Python Global Interpreter Lock
            py::gil_scoped_acquire acquire;
            
            //The Script assigned with the PyScript
            Script* script = nullptr;
            try {
                py::module_ py_module;
                try {
                    std::filesystem::path exeDir = getExecutableDirectory();
                    std::filesystem::path scriptDir = exeDir / "scripts";
                    std::filesystem::path scriptPath = scriptDir / (moduleName + ".py");

                    if (!std::filesystem::exists(scriptPath)) {
                        std::cerr << "[PyInterpreter::AttachPyScript] ERROR: No Python script at path '" << scriptPath.string() << "'" << std::endl;
                        return nullptr; //File not found
                    }

                    py_module = py::module_::import(moduleName.c_str());
                } catch (const py::error_already_set& e) {
                    std::cerr << "[PyInterpreter::AttachPyScript] ERROR: PyScript import failed for path '" << moduleName << "' with Python error: " << e.what() << std::endl;
                    return nullptr; // Module not found or import error
                }

                py::object pyScriptFactoryMethod;
                try {
                    pyScriptFactoryMethod = py_module.attr(PYSCRIPT_FACTORY_METHOD.c_str());
                } catch (const py::error_already_set& e) {
                    std::cerr << "[PyInterpreter::AttachPyScript] ERROR: PyScript factory method '" << PYSCRIPT_FACTORY_METHOD << "' missing in module '" << moduleName << "'. Error: " << e.what() << std::endl;
                    std::cerr << "PyScript subclasses MUST override factory method '" << PYSCRIPT_FACTORY_METHOD << "' from PyScript base class '" << moduleName << "'" << std::endl;
                    return nullptr; // Factory function not found
                }

                if (!PyCallable_Check(pyScriptFactoryMethod.ptr())) {
                    std::cerr << "[PyInterpreter::AttachPyScript] ERROR: PyScript factory method '" << PYSCRIPT_FACTORY_METHOD << "' in module '" << moduleName << "' is not callable." << std::endl;
                    return nullptr; //Factory function not callable
                }

                py::object py_instance;
                try {
                    py_instance = pyScriptFactoryMethod(); // Call the factory method
                } catch (const py::error_already_set& e) {
                    std::cerr << "[PyInterpreter::AttachPyScript] ERROR: Python error in PyScript factory method '" << PYSCRIPT_FACTORY_METHOD << "' in module '" << moduleName << "'. Error: " << e.what() << std::endl;
                    return nullptr; // Error during instance creation
                }

                if (py_instance.is_none()) {
                    std::cerr << "[PyInterpreter::AttachPyScript] ERROR: PyScript factory method '" << PYSCRIPT_FACTORY_METHOD << "' in module '" << moduleName << "' returned None." << std::endl;
                    return nullptr; //Invalid instance from factory method
                }

                // GIL released automatically when 'acquire' goes out of scope
                return new Script(py_instance); //Return the constructed Script
            } catch (const std::exception& e) {
                std::cerr << "[PyInterpreter::AttachPyScript] C++ exception while importing PyScript. Error: " << e.what() << std::endl;
                if (script && !script->getId().has_value()) { // Check if it wasn't successfully added
                    delete script;
                }
                return nullptr;
            }

            // GIL released automatically when 'acquire' goes out of scope
            return nullptr;
        }
        
        //This is name of the PyScript factory method to be called
        const std::string PYSCRIPT_FACTORY_METHOD = "create_instance";
        
        PyInterpreter(const PyInterpreter&) = delete;
        PyInterpreter& operator=(const PyInterpreter&) = delete;
        PyInterpreter(PyInterpreter&&) = delete;
        PyInterpreter& operator=(PyInterpreter&&) = delete;
    private:
		pybind11::scoped_interpreter guard{};
	};
}