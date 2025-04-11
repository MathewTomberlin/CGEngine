#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

inline std::filesystem::path getExecutableDirectory() {
    // --- BEGIN PLATFORM-SPECIFIC IMPLEMENTATION ---
    // Example (using a hypothetical function, replace this!):
    // return MyApp::GetApplicationPath().parent_path();

    // Example (may work on some systems with C++17, but test thoroughly):
    // return std::filesystem::current_path(); // Often NOT the executable dir! Needs careful handling.

    // Placeholder - **REPLACE THIS WITH YOUR ACTUAL IMPLEMENTATION**
    std::cerr << "[InterpreterManager] Warning: getExecutableDirectory() not implemented. Using current directory as fallback: " << std::filesystem::current_path().string() << std::endl;
    return std::filesystem::current_path();
    // --- END PLATFORM-SPECIFIC IMPLEMENTATION ---
}

namespace CGEngine {
    namespace py = pybind11;

	class InterpreterManager {
    public:
        InterpreterManager() { 
            try {
                std::filesystem::path exeDir = getExecutableDirectory();
                std::filesystem::path bindingsDir = exeDir;
                std::filesystem::path userScriptsDir = exeDir / "resources" / "scripts";
				py::module_ sys = py::module_::import("sys");
				py::list sysPath = sys.attr("path");
                sysPath.append(bindingsDir.string());
                sysPath.append(userScriptsDir.string());
            } catch (const py::error_already_set& e) {
                throw;
			} catch (const std::exception& e) {
                std::cerr << "[InterpreterManager] Fatal C++ Error during initialization: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[InterpreterManager] Fatal Unknown Error during initialization." << std::endl;
                throw;
            }
        }
        ~InterpreterManager() = default;

        /**
         * @brief Executes a Python script file.
         *
         * @param scriptPath The absolute path to the Python script file,
         * or a path relative to one of the directories in sys.path
         * (like the configured 'resources/scripts' directory).
         * @return True if the script executed without raising an exception, False otherwise.
         */
        bool run(const std::string& scriptPath) {
            // Ensure the GIL is held by this thread
            py::gil_scoped_acquire acquire;

            try {
                // Use py::eval_file to execute the script.
                // It executes the file in the __main__ module's context.
                py::eval_file(scriptPath);

                // Check if a Python exception occurred *during* execution but was handled within Python
                if (PyErr_Occurred()) {
                    // This usually shouldn't happen if eval_file throws on error,
                    // but checking is robust.
                    py::error_already_set error; // Fetch the error
                    std::cerr << "[InterpreterManager] Python Error occurred during script '" << scriptPath << "': "
                        << error.what() << std::endl;
                    PyErr_Clear(); // Clear the error state
                    return false;
                }

                std::cout << "[InterpreterManager] Successfully executed script: " << scriptPath << std::endl;
                return true;

            }
            catch (const py::error_already_set& e) {
                // Catch exceptions thrown by py::eval_file (syntax errors, runtime exceptions)
                std::cerr << "[InterpreterManager] Failed to execute script '" << scriptPath << "'. Python Error: "
                    << e.what() << std::endl;
                // PyErr_Clear() is implicitly handled by error_already_set destructor
                return false;
            }
            catch (const std::exception& e) {
                // Catch C++ exceptions during the process
                std::cerr << "[InterpreterManager] C++ Error while trying to run script '" << scriptPath << "': "
                    << e.what() << std::endl;
                return false;
            }
            // GIL is automatically released when 'acquire' goes out of scope
        }
        InterpreterManager(const InterpreterManager&) = delete;
        InterpreterManager& operator=(const InterpreterManager&) = delete;
        InterpreterManager(InterpreterManager&&) = delete;
        InterpreterManager& operator=(InterpreterManager&&) = delete;
    private:
		pybind11::scoped_interpreter guard{};
	};
}