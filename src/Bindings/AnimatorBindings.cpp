#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // For std::string, std::vector
// If binding getBoneMatrices later, need glm and matrix conversion/binding
// #include <glm/glm.hpp>
// #include <pybind11/numpy.h> // Example for numpy conversion

#include "../Core/Animation/Animator.h"   // Definition of Animator
#include "../Core/Animation/Animation.h" // Needed? Maybe not directly
#include "../Core/Skeleton/Skeleton.h"   // For setSkeleton parameter type
#include "../Core/Types/Types.h"       // For EngineSystem if inheriting

namespace py = pybind11;

void bindAnimator(py::module_& m) {
    // Bind Animator class. Use nodelete assuming lifetime is managed elsewhere (e.g., by Model).
    py::class_<CGEngine::Animator, CGEngine::EngineSystem, std::unique_ptr<CGEngine::Animator, py::nodelete>>(m, "Animator")
        // Skip constructor binding (Animators likely created via Model::createAnimator)

        .def("play_animation", &CGEngine::Animator::playAnimation, py::arg("animation_name"),
            "Plays the animation with the given name.")

        .def("set_skeleton", &CGEngine::Animator::setSkeleton, py::arg("skeleton"),
            "Sets the skeleton for the animator to use.")

        // --- Deferred Methods ---
        // .def("get_bone_matrices", ...) // Deferred due to glm::mat4 return type complexity
        // .def("update_animation", ...) // Likely called internally by engine, not Python
        ;
}