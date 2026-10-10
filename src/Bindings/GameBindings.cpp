#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <nlohmann/json.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/System/Vector3.hpp>
#include <map>
#include <stdexcept>
#include "../Core/Engine/Engine.h"
#include "../Core/Body/Body.h"
#include "../Core/Camera/Camera.h"
#include "../Core/Mesh/Mesh.h"
#include "../Core/Scene/SceneLoader.h"
#include "../Core/World/Renderer.h"

namespace py = pybind11;
using namespace py::literals;
using namespace CGEngine;

// Helpers for Python-driven games: keyboard state, body lookup, scene fragments and the camera.
// Kept separate from CoreBindings so the surface is easy to find (see docs/games/dungeon.md).
namespace {
    // Keys a game can poll. Names are the ones Python scripts use.
    const std::map<std::string, sf::Keyboard::Scan>& gameKeys() {
        static const std::map<std::string, sf::Keyboard::Scan> keys = {
            {"W", sf::Keyboard::Scan::W}, {"A", sf::Keyboard::Scan::A}, {"S", sf::Keyboard::Scan::S}, {"D", sf::Keyboard::Scan::D},
            {"Up", sf::Keyboard::Scan::Up}, {"Down", sf::Keyboard::Scan::Down}, {"Left", sf::Keyboard::Scan::Left}, {"Right", sf::Keyboard::Scan::Right},
            {"J", sf::Keyboard::Scan::J}, {"K", sf::Keyboard::Scan::K}, {"Space", sf::Keyboard::Scan::Space}, {"Enter", sf::Keyboard::Scan::Enter},
            {"R", sf::Keyboard::Scan::R}, {"Escape", sf::Keyboard::Scan::Escape},
        };
        return keys;
    }

    Body* findBody(const std::string& name) {
        for (Body* body : assets.getAllResources<Body>()) {
            if (body->getName() == name) return body;
        }
        return nullptr;
    }
}

void bindGame(py::module_& m) {
    m.def("key_down", [](const std::string& name) {
        auto it = gameKeys().find(name);
        if (it == gameKeys().end()) throw std::invalid_argument("unknown key '" + name + "'");
        return sf::Keyboard::isKeyPressed(it->second);
    }, py::arg("name"), "True while the named key is held. Names: W A S D Up Down Left Right J K Space Enter R Escape.");

    m.def("find_body", [](const std::string& name) -> Body* {
        return findBody(name);
    }, py::arg("name"), py::return_value_policy::reference, "The first body with this name, or None.");

    m.def("load_scene_json", [](const std::string& text) {
        SceneLoadResult result = SceneLoader::loadJson(nlohmann::json::parse(text));
        if (!result.ok) throw std::runtime_error(result.error);
        return py::dict("bodies"_a = result.bodies, "lights"_a = result.lights, "materials"_a = result.materials, "scripts"_a = result.scripts);
    }, py::arg("text"), "Load a scene given as JSON text, added to the world. Same format as scene files.");

    m.def("remove_body", [](const std::string& name) {
        Body* body = findBody(name);
        if (!body || body == world->getRoot()) return false;
        body->deleteBody(ChildrenTermination::Terminate);
        return true;
    }, py::arg("name"), "Remove a body and its children. Returns False if there is no such body.");

    m.def("set_camera", [](sf::Vector3f position, sf::Vector3f target) {
        if (!renderer) throw std::runtime_error("the renderer is not running yet");
        Camera* camera = renderer->getCurrentCamera();
        // Camera::setPosition negates y (its 2D convention), so pass -y to keep world coordinates.
        camera->setPosition(sf::Vector3f(position.x, -position.y, position.z), false);
        camera->lookAt(target);
    }, py::arg("position"), py::arg("target"), "Place the camera at position (world coordinates, y up) and aim it at target.");
}
