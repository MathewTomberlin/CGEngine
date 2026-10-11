#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <nlohmann/json.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector3.hpp>
#include <map>
#include <optional>
#include "../Core/Light/Light.h"
#include <cstring>
#include <stdexcept>
#include "../Core/Engine/Engine.h"
#include "../Core/Body/Body.h"
#include "../Core/Camera/Camera.h"
#include "../Core/Mesh/Mesh.h"
#include "../Core/Mesh/Model.h"
#include "../Core/Material/Material.h"
#include "../Core/Mesh/RuntimeMesh.h"
#include "../Core/Animation/Animator.h"
#include <algorithm>
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
            {"1", sf::Keyboard::Scan::Num1}, {"2", sf::Keyboard::Scan::Num2}, {"3", sf::Keyboard::Scan::Num3},
            {"4", sf::Keyboard::Scan::Num4}, {"5", sf::Keyboard::Scan::Num5}, {"6", sf::Keyboard::Scan::Num6},
            {"7", sf::Keyboard::Scan::Num7}, {"8", sf::Keyboard::Scan::Num8}, {"9", sf::Keyboard::Scan::Num9},
            {"F5", sf::Keyboard::Scan::F5}, {"F9", sf::Keyboard::Scan::F9},
        };
        return keys;
    }

    // Mouse look: while captured, the cursor is hidden and kept at the window's centre, and mouse_delta reports how far
    // it moved since the last call.
    bool mouseCaptured = false;

    RenderWindow& gameWindow() {
        if (!renderer || !renderer->getWindow()) throw std::runtime_error("the window is not open yet");
        return *renderer->getWindow();
    }

    sf::Vector2i windowCentre(const RenderWindow& window) {
        const sf::Vector2u size = window.getSize();
        return sf::Vector2i(static_cast<int>(size.x / 2), static_cast<int>(size.y / 2));
    }

    Body* findBody(const std::string& name) {
        for (Body* body : assets.getAllResources<Body>()) {
            if (body->getName() == name) return body;
        }
        return nullptr;
    }

    // Copies a one-dimensional buffer (array.array, numpy, bytes) into a vector. The element size must match T.
    template <typename T>
    std::vector<T> copyBuffer(const py::buffer& buffer, const char* what) {
        py::buffer_info info = buffer.request();
        if (info.ndim > 1 || info.itemsize != static_cast<py::ssize_t>(sizeof(T))) {
            throw std::invalid_argument(std::string("'") + what + "' must be a flat array of " + std::to_string(sizeof(T)) + "-byte numbers");
        }
        std::vector<T> data(static_cast<size_t>(info.size));
        if (!data.empty()) std::memcpy(data.data(), info.ptr, data.size() * sizeof(T));
        return data;
    }

    struct RuntimeMeshArrays {
        std::vector<VertexData> vertices;
        std::vector<unsigned int> indices;
    };

    // Builds vertices from flat arrays. Per vertex: positions 3, normals 3, colours 3, uvs 2, material ids 1. Normals,
    // colours, uvs and material ids may be left empty (up, white, zero and material 0).
    RuntimeMeshArrays buildRuntimeMesh(py::buffer positions, py::buffer normals, py::buffer colours, py::buffer uvs,
                                       py::buffer materialIds, py::buffer indices) {
        const auto pos = copyBuffer<float>(positions, "positions");
        const auto nrm = copyBuffer<float>(normals, "normals");
        const auto col = copyBuffer<float>(colours, "colours");
        const auto uv = copyBuffer<float>(uvs, "uvs");
        const auto mat = copyBuffer<float>(materialIds, "material_ids");
        if (pos.size() % 3 != 0) throw std::invalid_argument("'positions' must hold 3 numbers per vertex");
        const size_t count = pos.size() / 3;
        auto expect = [count](size_t size, size_t perVertex, const char* what) {
            if (size != 0 && size != count * perVertex) {
                throw std::invalid_argument(std::string("'") + what + "' must hold " + std::to_string(perVertex) +
                    " numbers per vertex (" + std::to_string(count) + " vertices), or be empty");
            }
        };
        expect(nrm.size(), 3, "normals");
        expect(col.size(), 3, "colours");
        expect(uv.size(), 2, "uvs");
        expect(mat.size(), 1, "material_ids");

        RuntimeMeshArrays mesh;
        mesh.vertices.resize(count);
        for (size_t i = 0; i < count; ++i) {
            VertexData& vertex = mesh.vertices[i];
            vertex.position = glm::vec3(pos[3 * i], pos[3 * i + 1], pos[3 * i + 2]);
            vertex.normal = nrm.empty() ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(nrm[3 * i], nrm[3 * i + 1], nrm[3 * i + 2]);
            vertex.colour = col.empty() ? glm::vec3(1.0f) : glm::vec3(col[3 * i], col[3 * i + 1], col[3 * i + 2]);
            vertex.texCoord = uv.empty() ? glm::vec2(0.0f) : glm::vec2(uv[2 * i], uv[2 * i + 1]);
            vertex.materialId = mat.empty() ? 0.0f : mat[i];
        }
        mesh.indices = copyBuffer<unsigned int>(indices, "indices");
        if (mesh.indices.size() % 3 != 0) throw std::invalid_argument("'indices' must hold 3 indices per triangle");
        for (unsigned int index : mesh.indices) {
            if (index >= count) throw std::invalid_argument("index " + std::to_string(index) + " is outside the " + std::to_string(count) + " vertices");
        }
        return mesh;
    }
}

void bindGame(py::module_& m) {
    m.def("key_down", [](const std::string& name) {
        auto it = gameKeys().find(name);
        if (it == gameKeys().end()) throw std::invalid_argument("unknown key '" + name + "'");
        return sf::Keyboard::isKeyPressed(it->second);
    }, py::arg("name"), "True while the named key is held. Names: W A S D Up Down Left Right J K Space Enter R Escape, 1 to 9, F5, F9.");

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

    m.def("play_clip", [](const std::string& name, const std::string& clip, bool looping, float speed) {
        Body* body = findBody(name);
        if (!body) throw std::runtime_error("no body named '" + name + "'");
        Model* model = nullptr;
        body->apply([&model](Body* b) {
            if (model) return;
            if (Mesh* mesh = b->get<Mesh*>()) {
                if (mesh->getModelId().has_value()) model = assets.get<Model>(mesh->getModelId().value());
            }
        });
        Animator* animator = model ? model->getAnimator() : nullptr;
        if (!animator) throw std::runtime_error("body '" + name + "' is not part of a model with animation clips");
        const auto& clips = model->getAnimationNames();
        if (std::find(clips.begin(), clips.end(), clip) == clips.end()) throw std::invalid_argument("no clip named '" + clip + "' on body '" + name + "'");
        animator->playAnimation(clip);
        animator->setSpeed(speed);
        animator->setLooping(looping);
        animator->setPaused(false);
    }, py::arg("name"), py::arg("clip"), py::arg("looping") = true, py::arg("speed") = 1.0f,
        "Start a clip on the model that a body belongs to. Restarts the clip if it is already playing.");
    m.def("create_mesh_body", [](const std::string& name, py::buffer positions, py::buffer normals, py::buffer colours,
                                  py::buffer uvs, py::buffer materialIds, py::buffer indices, std::vector<std::string> materials,
                                  sf::Vector3f position) {
        if (findBodyByName(name)) throw std::invalid_argument("a body named '" + name + "' already exists");
        RuntimeMeshArrays arrays = buildRuntimeMesh(positions, normals, colours, uvs, materialIds, indices);
        std::vector<id_t> materialList;
        for (const std::string& materialName : materials) {
            optional<id_t> materialId = assets.getId<Material>(materialName);
            if (!materialId.has_value()) throw std::invalid_argument("unknown material '" + materialName + "'");
            materialList.push_back(materialId.value());
        }
        if (materialList.empty()) materialList.push_back(assets.getDefaultId<Material>().value_or(0));
        if (!createRuntimeMeshBody(name, std::move(arrays.vertices), std::move(arrays.indices), materialList, position)) {
            throw std::runtime_error("failed to create body '" + name + "'");
        }
    }, py::arg("name"), py::arg("positions"), py::arg("normals"), py::arg("colours"), py::arg("uvs"),
        py::arg("material_ids"), py::arg("indices"), py::arg("materials") = std::vector<std::string>{},
        py::arg("position") = sf::Vector3f(0, 0, 0),
        "Make a body from flat arrays (array.array: 'f' for vertices, 'I' for indices). Empty normals, colours, uvs or material_ids use defaults.");

    m.def("update_mesh_body", [](const std::string& name, py::buffer positions, py::buffer normals, py::buffer colours,
                                  py::buffer uvs, py::buffer materialIds, py::buffer indices) {
        Body* body = findBodyByName(name);
        if (!body) throw std::invalid_argument("no body named '" + name + "'");
        RuntimeMeshArrays arrays = buildRuntimeMesh(positions, normals, colours, uvs, materialIds, indices);
        if (!updateRuntimeMeshBody(body, std::move(arrays.vertices), std::move(arrays.indices))) {
            throw std::invalid_argument("body '" + name + "' was not made by create_mesh_body");
        }
    }, py::arg("name"), py::arg("positions"), py::arg("normals"), py::arg("colours"), py::arg("uvs"),
        py::arg("material_ids"), py::arg("indices"),
        "Replace the vertices and indices of a body made by create_mesh_body. The GPU copy is updated before the next frame.");

    m.def("remove_mesh_body", [](const std::string& name) {
        return removeRuntimeMeshBody(name);
    }, py::arg("name"), "Remove a body made by create_mesh_body, and free its mesh. Returns False if there is no such body.");

    m.def("mouse_down", [](const std::string& name) {
        if (name == "Left") return sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        if (name == "Right") return sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
        if (name == "Middle") return sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle);
        throw std::invalid_argument("unknown mouse button '" + name + "' (use Left, Right or Middle)");
    }, py::arg("name"), "True while the named mouse button is held. Names: Left Right Middle.");

    m.def("set_mouse_captured", [](bool captured) {
        RenderWindow& window = gameWindow();
        mouseCaptured = captured;
        window.setMouseCursorVisible(!captured);
        if (captured) sf::Mouse::setPosition(windowCentre(window), window);
    }, py::arg("captured"), "Capture the mouse for mouse look: the cursor is hidden and kept at the window's centre. False releases it.");

    m.def("mouse_delta", []() -> std::pair<float, float> {
        if (!mouseCaptured) return { 0.0f, 0.0f };
        RenderWindow& window = gameWindow();
        const sf::Vector2i centre = windowCentre(window);
        const sf::Vector2i now = sf::Mouse::getPosition(window);
        sf::Mouse::setPosition(centre, window);
        return { static_cast<float>(now.x - centre.x), static_cast<float>(now.y - centre.y) };
    }, "How far the mouse moved since the last call, in pixels (x right, y down), while the mouse is captured. Otherwise (0, 0).");

    m.def("set_camera", [](sf::Vector3f position, sf::Vector3f target) {
        if (!renderer) throw std::runtime_error("the renderer is not running yet");
        Camera* camera = renderer->getCurrentCamera();
        // Camera::setPosition negates y (its 2D convention), so pass -y to keep world coordinates.
        camera->setPosition(sf::Vector3f(position.x, -position.y, position.z), false);
        camera->lookAt(target);
    }, py::arg("position"), py::arg("target"), "Place the camera at position (world coordinates, y up) and aim it at target.");

    m.def("set_environment", [](std::optional<sf::Vector3f> sky, std::optional<sf::Vector3f> fog, std::optional<float> fogStart,
        std::optional<float> fogEnd, std::optional<sf::Vector3f> zenith, std::optional<sf::Vector3f> sunDirection,
        std::optional<sf::Vector3f> sunColour, std::optional<float> sunSize) {
        if (!renderer) throw std::runtime_error("the renderer is not running yet");
        Environment& environment = renderer->environment;
        if (sky) environment.skyColour = *sky;
        if (fog) environment.fogColour = *fog;
        if (fogStart) environment.fogStart = *fogStart;
        if (fogEnd) environment.fogEnd = *fogEnd;
        if (zenith) environment.zenithColour = *zenith;
        if (sunDirection) environment.sunDirection = *sunDirection;
        if (sunColour) environment.sunColour = *sunColour;
        if (sunSize) environment.sunSize = *sunSize;
    }, py::arg("sky") = py::none(), py::arg("fog") = py::none(), py::arg("fog_start") = py::none(), py::arg("fog_end") = py::none(),
    py::arg("zenith") = py::none(), py::arg("sun_direction") = py::none(), py::arg("sun_colour") = py::none(),
    py::arg("sun_size") = py::none(),
    "The sky and fog. sky is the colour at the horizon, zenith the colour straight up (left out, the sky is one colour). fog fades from fog_start to fog_end blocks away; the fog is off while fog_end is not past fog_start. sun_direction points towards the sun, sun_colour draws its disc (black hides it) and sun_size is its angular radius in degrees. Arguments left out keep their value.");

    m.def("set_light", [](const std::string& name, std::optional<sf::Vector3f> direction, std::optional<sf::Vector3f> colour,
        std::optional<float> brightness, std::optional<float> ambiance) {
        Light* light = assets.get<Light>(name);
        if (!light) throw std::runtime_error("no light named '" + name + "'");
        // Renderer copies the parameters into the light buffer every frame, so the change applies at once.
        if (direction) light->parameters.lightDirection = *direction;
        if (colour) light->parameters.colorIntensities = *colour;
        if (brightness) light->parameters.brightness = *brightness;
        if (ambiance) light->parameters.ambiance = *ambiance;
    }, py::arg("name"), py::arg("direction") = py::none(), py::arg("colour") = py::none(), py::arg("brightness") = py::none(),
    py::arg("ambiance") = py::none(),
    "Change a light made by the scene: direction (where the light travels, for a directional light), colour, brightness and ambiance. Arguments left out keep their value.");

    m.def("camera_basis", []() {
        if (!renderer) throw std::runtime_error("the renderer is not running yet");
        Camera* camera = renderer->getCurrentCamera();
        auto vec = [](glm::vec3 v) { return py::make_tuple(v.x, v.y, v.z); };
        py::dict basis;
        basis["forward"] = vec(camera->getForward());
        basis["right"] = vec(camera->getRight());
        basis["up"] = vec(camera->getUp());
        return basis;
    }, "The camera's forward, right and up directions in world coordinates. Right stays level (y = 0) however the view is turned.");
}
