#include <pybind11/pybind11.h>
#include <pybind11/operators.h> // For binding operators like +,-,*,/

// Include necessary SFML and Engine headers
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Angle.hpp>
#include "../Core/Types/Types.h"

namespace py = pybind11;

// Define the function that binds math/SFML types
void bindMathTypes(py::module_& m) {
    // sf::Vector2f
    py::class_<sf::Vector2f>(m, "Vector2f")
        .def(py::init<float, float>(), py::arg("x") = 0.0f, py::arg("y") = 0.0f) // Default constructor
        .def_readwrite("x", &sf::Vector2f::x)
        .def_readwrite("y", &sf::Vector2f::y)
        .def(py::self + py::self) // Vector addition
        .def(py::self - py::self) // Vector subtraction
        .def(py::self * float())  // Scale by float (right side)
        .def(float() * py::self)  // Scale by float (left side)
        .def(py::self / float())  // Divide by float
        .def(-py::self)           // Unary minus
        .def("__repr__", [](const sf::Vector2f& v) {
        return "<Vector2f x=" + std::to_string(v.x) + " y=" + std::to_string(v.y) + ">";
            });
    // Note: We don't explicitly bind CGEngine::V2f as it inherits publicly
    //       and doesn't add Python-specific functionality *yet*.
    //       Python can use Vector2f for now.

// sf::Vector3f
    py::class_<sf::Vector3f>(m, "Vector3f")
        .def(py::init<float, float, float>(), py::arg("x") = 0.0f, py::arg("y") = 0.0f, py::arg("z") = 0.0f)
        .def_readwrite("x", &sf::Vector3f::x)
        .def_readwrite("y", &sf::Vector3f::y)
        .def_readwrite("z", &sf::Vector3f::z)
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self * float())
        .def(float() * py::self)
        .def(py::self / float())
        .def(-py::self)
        .def("__repr__", [](const sf::Vector3f& v) {
        return "<Vector3f x=" + std::to_string(v.x) + " y=" + std::to_string(v.y) + " z=" + std::to_string(v.z) + ">";
            });
    // Note: Similarly skipping CGEngine::V3f for now.

// sf::Color
    py::class_<sf::Color>(m, "Color")
        .def(py::init<uint8_t, uint8_t, uint8_t, uint8_t>(), py::arg("r") = 0, py::arg("g") = 0, py::arg("b") = 0, py::arg("a") = 255)
        .def_readwrite("r", &sf::Color::r)
        .def_readwrite("g", &sf::Color::g)
        .def_readwrite("b", &sf::Color::b)
        .def_readwrite("a", &sf::Color::a)
        .def_static("White", []() { return sf::Color::White; }) // Expose common colors
        .def_static("Black", []() { return sf::Color::Black; })
        .def_static("Red", []() { return sf::Color::Red; })
        // Add others as needed (Green, Blue, Yellow, Magenta, Cyan, Transparent)
        .def(py::self == py::self) // Equality comparison
        .def(py::self != py::self) // Inequality comparison
        .def("__repr__", [](const sf::Color& c) {
        return "<Color r=" + std::to_string(c.r) + " g=" + std::to_string(c.g) + " b=" + std::to_string(c.b) + " a=" + std::to_string(c.a) + ">";
            });

    // sf::Angle
    py::class_<sf::Angle>(m, "Angle")
        .def(py::init<>()) // Default constructor (zero angle)
        .def("as_degrees", &sf::Angle::asDegrees)
        .def("as_radians", &sf::Angle::asRadians)
        // Operators
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self * float())
        .def(float() * py::self)
        .def(py::self / float())
        .def(-py::self)
        .def("__repr__", [](const sf::Angle& a) {
        return "<Angle degrees=" + std::to_string(a.asDegrees()) + ">";
            });

    // Bind the free factory functions sf::degrees and sf::radians
    // These will be available as cg_engine_bindings.degrees(...) and cg_engine_bindings.radians(...)
    m.def("degrees", &sf::degrees, "Create an Angle from degrees", py::arg("angle"));
    m.def("radians", &sf::radians, "Create an Angle from radians", py::arg("angle"));

    // sf::FloatRect (Corrected for SFML 3.0 API: position + size)
    py::class_<sf::FloatRect>(m, "FloatRect")
        // Constructor 1: Default (0,0,0,0)
        .def(py::init<>())
        // Constructor 2: Takes position vector and size vector
        .def(py::init<const sf::Vector2f&, const sf::Vector2f&>(),
            py::arg("position"), py::arg("size"))
        // Constructor 3: Takes individual float components (more Pythonic often)
        .def(py::init([](float left, float top, float width, float height) {
        return sf::FloatRect({ left, top }, { width, height });
            }),
            py::arg("left") = 0.0f, py::arg("top") = 0.0f, py::arg("width") = 0.0f, py::arg("height") = 0.0f)

        // Expose position and size as read-write members
        .def_readwrite("position", &sf::FloatRect::position)
        .def_readwrite("size", &sf::FloatRect::size)

        // Expose left, top, width, height as properties for convenience
        .def_property("left", [](const sf::FloatRect& r) { return r.position.x; },
            [](sf::FloatRect& r, float v) { r.position.x = v; })
        .def_property("top", [](const sf::FloatRect& r) { return r.position.y; },
            [](sf::FloatRect& r, float v) { r.position.y = v; })
        .def_property("width", [](const sf::FloatRect& r) { return r.size.x; },
            [](sf::FloatRect& r, float v) { r.size.x = v; })
        .def_property("height", [](const sf::FloatRect& r) { return r.size.y; },
            [](sf::FloatRect& r, float v) { r.size.y = v; })

        .def("contains", [](const sf::FloatRect& self, float x, float y) {
        return self.contains({ x, y });
            }, "Check if a point defined by floats is inside the rectangle", py::arg("x"), py::arg("y"))
        .def("contains", [](const sf::FloatRect& self, const sf::Vector2f& point) {
        return self.contains(point);
            }, "Check if a point defined by a Vector2f is inside the rectangle", py::arg("point"))

        // Keep the intersects method, ensuring correct C++ call
        // (Assuming SFML 3.0 still uses the output parameter style for intersection details)
        .def("intersects", [](const sf::FloatRect& self, const sf::FloatRect& other) -> std::optional<sf::FloatRect> {
        sf::FloatRect intersection; // SFML writes result here
        if (self.findIntersection(intersection)) { // Call the version that takes an output parameter
            return intersection; // Return the result if true
        }
        return std::nullopt; // Return None if false
            }, "Check intersection with another FloatRect and return the intersection rectangle or None", py::arg("other"))

        // Update __repr__ to use position and size
        .def("__repr__", [](const sf::FloatRect& r) {
        return "<FloatRect position=(" + std::to_string(r.position.x) + "," + std::to_string(r.position.y) +
            ") size=(" + std::to_string(r.size.x) + "," + std::to_string(r.size.y) + ")>";
            });
}