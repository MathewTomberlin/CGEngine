#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <string>
#include "../Core/Engine/Engine.h"

namespace py = pybind11;

// --- Forward Declarations or Includes for Bind Functions ---
// As we create more binding files (e.g., MathBindings.cpp), we'll declare
// functions here that will be defined in those files and called within the module definition.
void bindMathTypes(py::module_& m);
void bindScriptTypes(py::module_& m);
void bindMesh(py::module_& m);
void bindTimeTypes(py::module_& m);
void bindMaterial(py::module_& m);
void bindProgram(py::module_& m);
void bindTexture(py::module_& m);
void bindFont(py::module_& m);
void bindAssetManager(py::module_& m);
void bindBehavior(py::module_& m);
void bindSkeleton(py::module_& m);
void bindAnimation(py::module_& m);
void bindAnimator(py::module_& m);
void bindCoreTypes(py::module_& m);
void bindModel(py::module_& m);
void bindWorld(py::module_& m);
void bindGame(py::module_& m);

// The bindings are embedded in main.exe, not built as a separate .pyd. A .pyd would carry its own copy
// of the engine's globals (assets, world, renderer), so scripts would see a different world from the one
// being drawn. Embedded modules share the engine's state. The module is registered before the interpreter
// starts, so `import cg_engine_bindings` finds it without a file on sys.path.
//   - First argument (cg_engine_bindings): the module name scripts import.
//   - Second argument (m): the Python module object.
bool cgEngineBindingsLinked = true; // referenced by PyInterpreter so the linker keeps this file

PYBIND11_EMBEDDED_MODULE(cg_engine_bindings, m) {

    m.doc() = "Python bindings for the CG Engine";

    // We will add calls here as we implement bindings in separate files.
    bindCoreTypes(m);
    bindMathTypes(m);
    bindScriptTypes(m);
    bindMesh(m);
    bindTimeTypes(m);
    bindMaterial(m);
    bindProgram(m);
    bindTexture(m);
    bindFont(m);
    bindAssetManager(m);
    bindBehavior(m);
    bindSkeleton(m);
    bindAnimation(m);
    bindAnimator(m);
    bindModel(m);
    bindWorld(m);
    bindGame(m);
    //extern CGEngine::PyInterpreter* interpreter;
    //m.attr("py_interpreter") = py::cast(*interpreter);
}