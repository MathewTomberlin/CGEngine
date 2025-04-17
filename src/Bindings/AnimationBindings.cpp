#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../Core/Animation/Animation.h"
#include "../Core/Types/Types.h"
#include "../Core/Mesh/Bone.h"
#include "../Core/Importer/MeshImporter.h"

namespace py = pybind11;

void bindAnimation(py::module_& m) {

    // Bind Bone for Animation::findBone return type
    py::class_<CGEngine::Bone, std::unique_ptr<CGEngine::Bone, py::nodelete>>(m, "Bone")
        // Constructor not bound, Bones are typically managed internally by Animation
        .def("get_bone_name", &CGEngine::Bone::getBoneName, "Get the name of the bone.")
        .def("get_bone_id", &CGEngine::Bone::getBoneId, "Get the ID of the bone.")
        // Defer binding getLocalTransform (returns glm::mat4)
        .def("__repr__", [](const CGEngine::Bone& b) {
        return "<Bone name='" + b.getBoneName() + ">";
            });

    // Bind NodeData for Animation::getRoot return type
    py::class_<CGEngine::NodeData>(m, "NodeData")
        .def(py::init<>()) // Bind default constructor if needed
        .def_readonly("name", &CGEngine::NodeData::name, "Name of the node.")
        .def_readonly("children_count", &CGEngine::NodeData::childrenCount, "Number of child nodes.")
        // Defer binding 'transformation' (glm::mat4)
        // Defer binding 'children' (vector<NodeData> - recursive/complex)
        .def("__repr__", [](const CGEngine::NodeData& n) {
        return "<NodeData name='" + n.name + "' children=" + std::to_string(n.childrenCount) + ">";
            });


    // Bind Animation
    py::class_<CGEngine::Animation, CGEngine::IResource, std::unique_ptr<CGEngine::Animation, py::nodelete>>(m, "Animation")
        // Skip constructor - Animations likely loaded/created via AssetManager/Model
        .def_property_readonly("id", &CGEngine::Animation::getId, "Get the unique ID of the animation resource.")
        .def("get_name", &CGEngine::Animation::getName, "Get the name of the animation.")
        .def("set_name", &CGEngine::Animation::setName, py::arg("name"), "Set the name of the animation.")
        .def("get_duration", &CGEngine::Animation::getDuration, "Get the duration of the animation in ticks.")
        .def("get_ticks_per_second", &CGEngine::Animation::getTicksPerSecond, "Get the number of ticks per second.")
        .def("is_valid", &CGEngine::Animation::isValid, "Check if the animation data is valid.")

        .def("find_bone", &CGEngine::Animation::findBone, py::arg("bone_name"),
            py::return_value_policy::reference, // Return reference to internal Bone object
            "Find a bone within the animation by its name.")

        .def("get_root_node", py::overload_cast<>(&CGEngine::Animation::getRoot, py::const_), // Using the const version
            py::return_value_policy::reference, // Return a reference to the internal NodeData
            "Get the root node of the animation's node hierarchy.")

        // Skip binding importAnimationHierarchy, importAnimationBones (internal helpers)
        // Skip binding direct access to 'bones' vector and public members like duration/ticksPerSecond (use getters)
        ;
}