#include <pybind11/pybind11.h>
#include <SFML/Graphics/Texture.hpp>
#include "../Core/Types/Types.h"

namespace py = pybind11;

void bindTexture(py::module_& m) {
    py::class_<sf::Texture, std::unique_ptr<sf::Texture, py::nodelete>>(m, "Texture")
        .def_property_readonly("size", &sf::Texture::getSize, "Get the size of the texture in pixels.")
        // Add other sf::Texture methods if needed (e.g., getNativeHandle)
        .def("get_native_handle", &sf::Texture::getNativeHandle, "Get the underlying OpenGL texture handle.")
        ;

    // Bind TextureResource. Use nodelete as AssetManager owns it.
    py::class_<CGEngine::TextureResource, CGEngine::IResource, std::unique_ptr<CGEngine::TextureResource, py::nodelete>>(m, "TextureResource")
        .def_property_readonly("id", &CGEngine::TextureResource::getId, "Get the unique ID of the texture resource.")
        .def("get_texture", py::overload_cast<>(&CGEngine::TextureResource::getTexture, py::const_), py::return_value_policy::reference, "Get the underlying sf::Texture object.")
        ;
}