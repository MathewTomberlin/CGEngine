#include <pybind11/pybind11.h>
#include <SFML/Graphics/Font.hpp>
#include "../Core/Types/Types.h"

namespace py = pybind11;

void bindFont(py::module_& m) {
    py::class_<sf::Font, std::unique_ptr<sf::Font, py::nodelete>>(m, "Font")
        .def("get_info", &sf::Font::getInfo, "Get the font information.")
        ;

    // Bind FontResource. Use nodelete as AssetManager owns it.
    py::class_<CGEngine::FontResource, CGEngine::IResource, std::unique_ptr<CGEngine::FontResource, py::nodelete>>(m, "FontResource")
        .def_property_readonly("id", &CGEngine::FontResource::getId, "Get the unique ID of the font resource.")
        .def("get_font", py::overload_cast<>(&CGEngine::FontResource::getFont, py::const_), py::return_value_policy::reference, "Get the underlying sf::Font object.")
        ;

    // Bind sf::Font::Info struct if needed (returned by get_info)
    py::class_<sf::Font::Info>(m, "FontInfo")
        .def_readonly("family", &sf::Font::Info::family, "The font family name.")
        ;
}