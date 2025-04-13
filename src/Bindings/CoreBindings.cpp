#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <string>

namespace py = pybind11;

// --- Forward Declarations or Includes for Bind Functions ---
// As we create more binding files (e.g., MathBindings.cpp), we'll declare
// functions here that will be defined in those files and called within the module definition.
void bindMathTypes(py::module_& m);
void bindScriptTypes(py::module_& m);
void bindMesh(py::module_& m);

// PYBIND11_MODULE is the entry point. Python calls this when the module is imported.
// - First argument (cg_engine_bindings): MUST match the target name in src/Bindings/CMakeLists.txt
// - Second argument (m): Represents the Python module object.
PYBIND11_MODULE(cg_engine_bindings, m) {
    m.doc() = "Python bindings for the CG Engine";

    // We will add calls here as we implement bindings in separate files.
    bindMathTypes(m);
    bindScriptTypes(m);
    bindMesh(m);

}