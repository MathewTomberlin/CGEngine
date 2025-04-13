#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <SFML/System/Angle.hpp>
#include "../Core/Mesh/Mesh.h"

namespace py = pybind11;
void bindMesh(py::module_& m) {
    py::class_<CGEngine::Mesh, std::unique_ptr<CGEngine::Mesh, py::nodelete>>(m, "Mesh") // Note: Mesh isn't directly unique_ptr'd, but Body::entity is. Use nodelete.
        .def("set_position", &CGEngine::Mesh::setPosition, py::arg("pos"))
        .def("set_rotation", &CGEngine::Mesh::setRotation, py::arg("rot"))
        .def("set_scale", &CGEngine::Mesh::setScale, py::arg("scale"))
        .def("move", &CGEngine::Mesh::move, py::arg("delta"))
        .def("rotate", &CGEngine::Mesh::rotate, py::arg("delta"))
        .def("scale", &CGEngine::Mesh::scale, py::arg("delta"))
        ;
}