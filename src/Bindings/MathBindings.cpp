#include <pybind11/pybind11.h>
#include <pybind11/operators.h> // For binding operators like +,-,*,/

// Include necessary SFML and Engine headers
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/Graphics/Shape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Sprite.hpp>
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

    // --- (Optional but good practice) Bind sf::Drawable minimally if needed as a base ---
    py::class_<sf::Drawable, std::unique_ptr<sf::Drawable, py::nodelete>>(m, "Drawable");
    // No methods needed for now, just register the type

    py::class_<sf::Transformable, std::unique_ptr<sf::Transformable, py::nodelete>>(m, "Transformable")
        // Add common Transformable methods now or later as needed
        .def("set_position", py::overload_cast<sf::Vector2f>(&sf::Transformable::setPosition), py::arg("position"))
        .def("get_position", &sf::Transformable::getPosition)
        .def("set_rotation", py::overload_cast<sf::Angle>(&sf::Transformable::setRotation), py::arg("angle"))
        .def("get_rotation", &sf::Transformable::getRotation)
        .def("set_scale", py::overload_cast<sf::Vector2f>(&sf::Transformable::setScale), py::arg("factors"))
        .def("get_scale", &sf::Transformable::getScale)
        .def("set_origin", py::overload_cast<sf::Vector2f>(&sf::Transformable::setOrigin), py::arg("origin"))
        .def("get_origin", &sf::Transformable::getOrigin)
        .def("move", py::overload_cast<sf::Vector2f>(&sf::Transformable::move), py::arg("offset"))
        .def("rotate", &sf::Transformable::rotate, py::arg("angle"))
        .def("scale", py::overload_cast<sf::Vector2f>(&sf::Transformable::scale), py::arg("factors"))
        .def("get_transform", &sf::Transformable::getTransform)
        .def("get_inverse_transform", &sf::Transformable::getInverseTransform);

    // Bind sf::Shape (Base for CircleShape, RectangleShape, etc.)
    py::class_<sf::Shape, sf::Transformable, std::unique_ptr<sf::Shape, py::nodelete>>(m, "Shape")
        .def("set_texture", [](sf::Shape& self, const sf::Texture& texture, bool resetRect) { self.setTexture(&texture, resetRect); }, 
            py::arg("texture"), py::arg("reset_rect") = false, "Set the source texture.")
        .def("set_fill_color", &sf::Shape::setFillColor, py::arg("color"), "Set the fill color.")
        .def("set_outline_color", &sf::Shape::setOutlineColor, py::arg("color"), "Set the outline color.")
        ;

    // Bind sf::Text
    py::class_<sf::Text, sf::Drawable, sf::Transformable, std::unique_ptr<sf::Text, py::nodelete>>(m, "Text")
        .def("set_string", static_cast<void (sf::Text::*)(const sf::String&)>(&sf::Text::setString), py::arg("string"), "Set the text string.")
        .def("set_font", [](sf::Text& self, const sf::Font& font) { self.setFont(font); }, py::arg("font"), "Set the font.")
        .def("set_character_size", &sf::Text::setCharacterSize, py::arg("size"), "Set the character size.")
        .def("set_fill_color", &sf::Text::setFillColor, py::arg("color"), "Set the fill color.")
        ;

    // Bind sf::Sprite
    py::class_<sf::Sprite, sf::Drawable, sf::Transformable, std::unique_ptr<sf::Sprite, py::nodelete>>(m, "Sprite")
        .def("set_texture", [](sf::Sprite& self, const sf::Texture& texture, bool resetRect) { self.setTexture(texture, resetRect); }, py::arg("texture"), py::arg("reset_rect") = false, "Set the source texture.")
        .def("set_texture_rect", &sf::Sprite::setTextureRect, py::arg("rectangle"), "Set the texture rectangle.")
        .def("set_color", &sf::Sprite::setColor, py::arg("color"), "Set the global color (multiplies with texture).")
        ;

    py::class_<CGEngine::Transformation3D>(m, "Transformation3D")
        .def(py::init<>(), "Default constructor (position=0,0,0, rotation=0,0,0, scale=1,1,1)")
        // Bind constructor: Transformation3D(Vector3f position, Vector3f rotation, Vector3f scale)
        .def(py::init<sf::Vector3f, sf::Vector3f, sf::Vector3f>(),
            py::arg("position") = sf::Vector3f(0, 0, 0), // Add defaults matching C++ if possible
            py::arg("rotation") = sf::Vector3f(0, 0, 0),
            py::arg("scale") = sf::Vector3f(1, 1, 1),
            "Constructor taking position, rotation (degrees), and scale.")
        // Bind public members as read-write attributes
        .def_readwrite("position", &CGEngine::Transformation3D::position)
        .def_readwrite("rotation", &CGEngine::Transformation3D::rotation, "Rotation in Euler angles (degrees)")
        .def_readwrite("scale", &CGEngine::Transformation3D::scale)
        // Add a representation method for easier debugging in Python
        .def("__repr__",
            [](const CGEngine::Transformation3D& t) {
                return "<Transformation3D pos=(" + std::to_string(t.position.x) + "," + std::to_string(t.position.y) + "," + std::to_string(t.position.z) +
                    ") rot=(" + std::to_string(t.rotation.x) + "," + std::to_string(t.rotation.y) + "," + std::to_string(t.rotation.z) +
                    ") scale=(" + std::to_string(t.scale.x) + "," + std::to_string(t.scale.y) + "," + std::to_string(t.scale.z) + ")>";
            });

    //py::enum_<sf::Keyboard::Scan>(m, "Scan", "Keyboard scancodes") // Note: SFML 3 uses sf::Keyboard::Scan::Scancode
    //    .value("Unknown", sf::Keyboard::Scan::Unknown)
    //    .value("A", sf::Keyboard::Scan::A)
    //    .value("B", sf::Keyboard::Scan::B)
    //    .value("C", sf::Keyboard::Scan::C)
    //    .value("D", sf::Keyboard::Scan::D)
    //    .value("E", sf::Keyboard::Scan::E)
    //    .value("F", sf::Keyboard::Scan::F)
    //    .value("G", sf::Keyboard::Scan::G)
    //    .value("H", sf::Keyboard::Scan::H)
    //    .value("I", sf::Keyboard::Scan::I)
    //    .value("J", sf::Keyboard::Scan::J)
    //    .value("K", sf::Keyboard::Scan::K)
    //    .value("L", sf::Keyboard::Scan::L)
    //    .value("M", sf::Keyboard::Scan::M)
    //    .value("N", sf::Keyboard::Scan::N)
    //    .value("O", sf::Keyboard::Scan::O)
    //    .value("P", sf::Keyboard::Scan::P)
    //    .value("Q", sf::Keyboard::Scan::Q)
    //    .value("R", sf::Keyboard::Scan::R)
    //    .value("S", sf::Keyboard::Scan::S)
    //    .value("T", sf::Keyboard::Scan::T)
    //    .value("U", sf::Keyboard::Scan::U)
    //    .value("V", sf::Keyboard::Scan::V)
    //    .value("W", sf::Keyboard::Scan::W)
    //    .value("X", sf::Keyboard::Scan::X)
    //    .value("Y", sf::Keyboard::Scan::Y)
    //    .value("Z", sf::Keyboard::Scan::Z)
    //    .value("Num1", sf::Keyboard::Scan::Num1)
    //    .value("Num2", sf::Keyboard::Scan::Num2)
    //    .value("Num3", sf::Keyboard::Scan::Num3)
    //    .value("Num4", sf::Keyboard::Scan::Num4)
    //    .value("Num5", sf::Keyboard::Scan::Num5)
    //    .value("Num6", sf::Keyboard::Scan::Num6)
    //    .value("Num7", sf::Keyboard::Scan::Num7)
    //    .value("Num8", sf::Keyboard::Scan::Num8)
    //    .value("Num9", sf::Keyboard::Scan::Num9)
    //    .value("Num0", sf::Keyboard::Scan::Num0)
    //    .value("Enter", sf::Keyboard::Scan::Enter)
    //    .value("Escape", sf::Keyboard::Scan::Escape)
    //    .value("Backspace", sf::Keyboard::Scan::Backspace)
    //    .value("Tab", sf::Keyboard::Scan::Tab)
    //    .value("Space", sf::Keyboard::Scan::Space)
    //    .value("Hyphen", sf::Keyboard::Scan::Hyphen)
    //    .value("Equal", sf::Keyboard::Scan::Equal)
    //    .value("LBracket", sf::Keyboard::Scan::LBracket)
    //    .value("RBracket", sf::Keyboard::Scan::RBracket)
    //    .value("Backslash", sf::Keyboard::Scan::Backslash)
    //    .value("Semicolon", sf::Keyboard::Scan::Semicolon)
    //    .value("Apostrophe", sf::Keyboard::Scan::Apostrophe)
    //    .value("Grave", sf::Keyboard::Scan::Grave)
    //    .value("Comma", sf::Keyboard::Scan::Comma)
    //    .value("Period", sf::Keyboard::Scan::Period)
    //    .value("Slash", sf::Keyboard::Scan::Slash)
    //    .value("F1", sf::Keyboard::Scan::F1)
    //    .value("F2", sf::Keyboard::Scan::F2)
    //    .value("F3", sf::Keyboard::Scan::F3)
    //    .value("F4", sf::Keyboard::Scan::F4)
    //    .value("F5", sf::Keyboard::Scan::F5)
    //    .value("F6", sf::Keyboard::Scan::F6)
    //    .value("F7", sf::Keyboard::Scan::F7)
    //    .value("F8", sf::Keyboard::Scan::F8)
    //    .value("F9", sf::Keyboard::Scan::F9)
    //    .value("F10", sf::Keyboard::Scan::F10)
    //    .value("F11", sf::Keyboard::Scan::F11)
    //    .value("F12", sf::Keyboard::Scan::F12)
    //    .value("F13", sf::Keyboard::Scan::F13)
    //    .value("F14", sf::Keyboard::Scan::F14)
    //    .value("F15", sf::Keyboard::Scan::F15)
    //    .value("F16", sf::Keyboard::Scan::F16)
    //    .value("F17", sf::Keyboard::Scan::F17)
    //    .value("F18", sf::Keyboard::Scan::F18)
    //    .value("F19", sf::Keyboard::Scan::F19)
    //    .value("F20", sf::Keyboard::Scan::F20)
    //    .value("F21", sf::Keyboard::Scan::F21)
    //    .value("F22", sf::Keyboard::Scan::F22)
    //    .value("F23", sf::Keyboard::Scan::F23)
    //    .value("F24", sf::Keyboard::Scan::F24)
    //    .value("CapsLock", sf::Keyboard::Scan::CapsLock)
    //    .value("PrintScreen", sf::Keyboard::Scan::PrintScreen)
    //    .value("ScrollLock", sf::Keyboard::Scan::ScrollLock)
    //    .value("Pause", sf::Keyboard::Scan::Pause)
    //    .value("Insert", sf::Keyboard::Scan::Insert)
    //    .value("Home", sf::Keyboard::Scan::Home)
    //    .value("PageUp", sf::Keyboard::Scan::PageUp)
    //    .value("Delete", sf::Keyboard::Scan::Delete)
    //    .value("End", sf::Keyboard::Scan::End)
    //    .value("PageDown", sf::Keyboard::Scan::PageDown)
    //    .value("Left", sf::Keyboard::Scan::Left)
    //    .value("Right", sf::Keyboard::Scan::Right)
    //    .value("Up", sf::Keyboard::Scan::Up)
    //    .value("Down", sf::Keyboard::Scan::Down)
    //    .value("LControl", sf::Keyboard::Scan::LControl)
    //    .value("LShift", sf::Keyboard::Scan::LShift)
    //    .value("LAlt", sf::Keyboard::Scan::LAlt)
    //    .value("LSystem", sf::Keyboard::Scan::LSystem) // Left OS key
    //    .value("RControl", sf::Keyboard::Scan::RControl)
    //    .value("RShift", sf::Keyboard::Scan::RShift)
    //    .value("RAlt", sf::Keyboard::Scan::RAlt)
    //    .value("RSystem", sf::Keyboard::Scan::RSystem) // Right OS key
    //    .value("Menu", sf::Keyboard::Scan::Menu)
    //    .export_values(); // Makes Scan.Space etc. available
    //
    //// Bind sf::Mouse::Button
    //py::enum_<sf::Mouse::Button>(m, "Button", "Mouse buttons")
    //    .value("Left", sf::Mouse::Button::Left)
    //    .value("Right", sf::Mouse::Button::Right)
    //    .value("Middle", sf::Mouse::Button::Middle)
    //    .value("XButton1", sf::Mouse::Button::Extra1) // Extra mouse button 1
    //    .value("XButton2", sf::Mouse::Button::Extra2) // Extra mouse button 2
    //    .export_values(); // Makes Button.Left etc. available

}