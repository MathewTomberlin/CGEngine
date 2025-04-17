#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../Core/Skeleton/Skeleton.h"
#include "../Core/Types/Types.h"
#include "../Core/Importer/MeshImporter.h"
#include "../Core/World/Renderer.h"

// Assuming glm types are handled via other bindings or conversion helpers if needed
// #include <glm/glm.hpp> // If binding glm types directly
namespace py = pybind11;

void bindSkeleton(py::module_& m) {

    // We need BoneData because Skeleton::getBoneData returns it.
    // Binding glm::mat4 directly is complex; for now, expose only the ID.
    py::class_<CGEngine::BoneData>(m, "BoneData")
        .def(py::init<>(), "Default constructor") // If needed
        .def_readonly("id", &CGEngine::BoneData::id, "The unique ID of the bone within the skeleton.")
        // .def_readonly("offset", ...) // Defer binding glm::mat4 offset matrix
        .def("__repr__", [](const CGEngine::BoneData& b) {
        return "<BoneData id=" + std::to_string(b.id) + ">"; // Simple representation
            });


    py::class_<CGEngine::Skeleton, CGEngine::IResource, std::unique_ptr<CGEngine::Skeleton, py::nodelete>>(m, "Skeleton")
        // Constructors likely handled by AssetManager, skip binding them directly.
        .def_property_readonly("id", &CGEngine::Skeleton::getId, "Get the unique ID of the skeleton resource.")
        .def("is_valid", &CGEngine::Skeleton::isValid,
            "Check if the skeleton is valid (e.g., has bone data).")
        .def("has_bone", &CGEngine::Skeleton::hasBone, py::arg("bone_name"),
            "Check if a bone with the given name exists in the skeleton.")
        .def("get_bone_data", &CGEngine::Skeleton::getBoneData, py::arg("bone_name"),
            "Get the BoneData for the specified bone name, or None if not found.")
        // Defer binding 'equals' method for now.
        ;
}