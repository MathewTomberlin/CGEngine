#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // For std::vector, std::optional
#include "../Core/Mesh/Model.h"
#include "../Core/Animation/Animator.h"
#include "../Core/Material/Material.h"
#include "../Core/Types/Types.h"

namespace py = pybind11;

void bindModel(py::module_& m) {
    py::class_<CGEngine::Model, CGEngine::IResource, std::unique_ptr<CGEngine::Model, py::nodelete>>(m, "Model")
        .def(py::init<std::string, std::string>(), py::arg("sourcePath"), py::arg("skeletonName") = "",
            "Constructor to create a Model from an imported file via Assimp")
        .def(py::init<CGEngine::MeshData*, std::string, std::string>(), py::arg("meshData"), py::arg("name") = "", py::arg("skeletonName") = "",
            "Constructor to create a Model manually, likely from MeshData")
        // Bind instantiate method
        .def("instantiate", &CGEngine::Model::instantiate, py::arg("rootTransform") = CGEngine::Transformation3D(), py::arg("overrideMaterials") = std::vector<CGEngine::id_t>(),
            "Create a hierarchy of Bodies from imported MeshNodeData and return the root Body id")
        // Bind getAnimator method
        .def("get_animator", &CGEngine::Model::getAnimator, py::return_value_policy::reference, "Return the model animator")
        // Bind getMaterials method
        .def("get_materials", &CGEngine::Model::getMaterials, py::return_value_policy::reference, "Return a vector of the model materials")
        // Bind isSkeletal property
        .def_property_readonly("is_skeletal", &CGEngine::Model::isSkeletal, "Return whether the model has bones")
        // Bind isValid method
        .def("is_valid", &CGEngine::Model::isValid, "Return whether the model is valid");
}