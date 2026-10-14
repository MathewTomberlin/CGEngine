#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <nlohmann/json.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/System/Vector3.hpp>
#include <cstdlib>
#include <array>
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
#include "../Core/Audio/AudioPlayer.h"

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
            {"0", sf::Keyboard::Scan::Num0}, {"-", sf::Keyboard::Scan::Hyphen}, {"=", sf::Keyboard::Scan::Equal},
            {"F5", sf::Keyboard::Scan::F5}, {"F9", sf::Keyboard::Scan::F9}, {"F12", sf::Keyboard::Scan::F12},
            {"`", sf::Keyboard::Scan::Grave}, {"Backspace", sf::Keyboard::Scan::Backspace}, {"Tab", sf::Keyboard::Scan::Tab},
            {"L", sf::Keyboard::Scan::L}, {"I", sf::Keyboard::Scan::I}, {"C", sf::Keyboard::Scan::C},
        };
        return keys;
    }

    // Mouse look: while captured, the cursor is hidden and kept at the window's centre, and mouse_delta reports how far
    // it moved since the last call.
    bool mouseCaptured = false;

    // CGENGINE_UNATTENDED=1 (the test scripts set it): no real keyboard, mouse or cursor input reaches the game, and the mouse is
    // never captured, so a run nobody is at the machine for is not steered by whoever is at it.
    bool unattended() {
        static const bool value = std::getenv("CGENGINE_UNATTENDED") != nullptr;
        return value;
    }

    // The sounds the game plays (AudioPlayer): one player for the process, made when it is first used.
    AudioPlayer& audioPlayer() {
        static AudioPlayer player;
        return player;
    }

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
        if (unattended()) return false;
        return sf::Keyboard::isKeyPressed(it->second);
    }, py::arg("name"), "True while the named key is held. Names: W A S D Up Down Left Right J K Space Enter Tab R Escape, 0 to 9, F5, F9, F12.");

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
        Animator* animator = body->findInstanceAnimator();
        if (!animator && model) animator = model->getAnimator();
        if (!animator) throw std::runtime_error("body '" + name + "' is not part of a model with animation clips");
        const auto& clips = model->getAnimationNames();
        if (std::find(clips.begin(), clips.end(), clip) == clips.end()) throw std::invalid_argument("no clip named '" + clip + "' on body '" + name + "'");
        animator->playAnimation(clip);
        animator->setSpeed(speed);
        animator->setLooping(looping);
        animator->setPaused(false);
    }, py::arg("name"), py::arg("clip"), py::arg("looping") = true, py::arg("speed") = 1.0f,
        "Start a clip on the model that a body belongs to. Restarts the clip if it is already playing. Each model instance "
        "(each body made from a model) plays on its own animator, so instances started at different times stay out of step.");
    m.def("animation_time", [](const std::string& name) {
        Body* body = findBody(name);
        if (!body) throw std::runtime_error("no body named '" + name + "'");
        Animator* animator = body->findInstanceAnimator();
        if (!animator) {
            Model* model = nullptr;
            body->apply([&model](Body* b) {
                if (model) return;
                if (Mesh* mesh = b->get<Mesh*>()) {
                    if (mesh->getModelId().has_value()) model = assets.get<Model>(mesh->getModelId().value());
                }
            });
            animator = model ? model->getAnimator() : nullptr;
        }
        if (!animator) throw std::runtime_error("body '" + name + "' is not part of a model with animation clips");
        return animator->getTimeSeconds();
    }, py::arg("name"), "Position in the body's current clip, in seconds (its model instance's clock).");
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

    m.def("set_xray", [](const std::string& name, bool on) {
        Body* body = findBodyByName(name);
        if (!body) throw std::invalid_argument("no body named '" + name + "'");
        body->xray = on;
    }, py::arg("name"), py::arg("on"), "Draw a body with the depth test off, so it shows through what is in front of it.");

    m.def("set_viewmodel", [](const std::string& name, bool on) {
        Body* body = findBodyByName(name);
        if (!body) throw std::invalid_argument("no body named '" + name + "'");
        body->viewmodel = on;
    }, py::arg("name"), py::arg("on"), "Draw a body last, after the depth buffer is cleared, so the world never clips it (a first-person hand).");

    m.def("remove_mesh_body", [](const std::string& name) {
        return removeRuntimeMeshBody(name);
    }, py::arg("name"), "Remove a body made by create_mesh_body, and free its mesh. Returns False if there is no such body.");

    m.def("set_mesh_body_position", [](const std::string& name, sf::Vector3f position) {
        Body* body = findBodyByName(name);
        if (!body) throw std::invalid_argument("no body named '" + name + "'");
        Mesh* mesh = body->get<Mesh*>();
        if (!mesh) throw std::invalid_argument("body '" + name + "' has no mesh to move");
        mesh->setPosition(position);
    }, py::arg("name"), py::arg("position"),
        "Move a body made by create_mesh_body. Its mesh is not rebuilt, so a body that only moves costs no upload.");

    m.def("mouse_down", [](const std::string& name) {
        if (name != "Left" && name != "Right" && name != "Middle") {
            throw std::invalid_argument("unknown mouse button '" + name + "' (use Left, Right or Middle)");
        }
        if (unattended()) return false;
        if (name == "Left") return sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        if (name == "Right") return sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
        return sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle);
    }, py::arg("name"), "True while the named mouse button is held. Names: Left Right Middle.");

    m.def("set_mouse_captured", [](bool captured) {
        if (unattended()) {
            mouseCaptured = false;   // the cursor is never taken: nothing is hidden, moved or read from it
            return;
        }
        RenderWindow& window = gameWindow();
        mouseCaptured = captured;
        window.setMouseCursorVisible(!captured);
        if (captured) sf::Mouse::setPosition(windowCentre(window), window);
    }, py::arg("captured"), "Capture the mouse for mouse look: the cursor is hidden and kept at the window's centre. False releases it.");

    m.def("mouse_position", []() -> std::pair<int, int> {
        const sf::Vector2i position = sf::Mouse::getPosition(gameWindow());
        return { position.x, position.y };
    }, "The mouse cursor's position in window pixels, from the top-left corner (it can be outside the window).");

    m.def("quit", []() { gameWindow().close(); }, "Close the window. The game ends when the current frame is done.");
    m.def("add_command", [](const std::string& name, py::function handler) {
        if (!interpreter) throw std::runtime_error("Python is not running");
        interpreter->addCommand(name, handler);
    }, py::arg("name"), py::arg("handler"),
       "Register a control command (see docs/ai/control-api.md). handler(params_json) gets the command's params as JSON text and "
       "returns its result as JSON text; a raised exception is reported as the command's error.");

    m.def("screenshot", [](const std::string& path) {
        if (!renderer) throw std::runtime_error("the renderer is not running yet");
        renderer->requestScreenshot(path);
    }, py::arg("path"), "Save the next frame as a PNG at path (absolute, or relative to the exe folder).");

    m.def("mouse_delta", []() -> std::pair<float, float> {
        if (!mouseCaptured) return { 0.0f, 0.0f };
        RenderWindow& window = gameWindow();
        const sf::Vector2i centre = windowCentre(window);
        const sf::Vector2i now = sf::Mouse::getPosition(window);
        sf::Mouse::setPosition(centre, window);
        return { static_cast<float>(now.x - centre.x), static_cast<float>(now.y - centre.y) };
    }, "How far the mouse moved since the last call, in pixels (x right, y down), while the mouse is captured. Otherwise (0, 0).");

    m.def("set_voxel_tiles", [](const std::string& path) {
        if (!renderer) throw std::runtime_error("the renderer is not running yet");
        renderer->setVoxelTiles(path);
    }, py::arg("path"), "Load the voxel tile array from a PNG strip of square tiles (one per block face). Voxel meshes use it for their surfaces.");

    m.def("set_voxel_splat_tiles", [](const std::vector<std::array<float, 3>>& tiles) {
        if (!renderer) throw std::runtime_error("the renderer is not running yet");
        renderer->setVoxelSplatTiles(tiles);
    }, py::arg("tiles"), "The top, side and bottom tile layers of the eight splat blocks, in channel order. The terrain blends them per pixel.");

    m.def("set_camera", [](sf::Vector3f position, sf::Vector3f target) {
        if (!renderer) throw std::runtime_error("the renderer is not running yet");
        Camera* camera = renderer->getCurrentCamera();
        // Camera::setPosition negates y (its 2D convention), so pass -y to keep world coordinates.
        camera->setPosition(sf::Vector3f(position.x, -position.y, position.z), false);
        camera->lookAt(target);
    }, py::arg("position"), py::arg("target"), "Place the camera at position (world coordinates, y up) and aim it at target.");

    m.def("typed_text", []() {
        // Only ASCII is kept: the console's commands are ASCII, and Backspace or Enter arrive as keys, not characters.
        std::string text;
        for (char32_t ch : InputMap::takeTypedText()) {
            if (ch < 128 && !unattended()) text.push_back(static_cast<char>(ch));
        }
        return text;
    }, "The characters typed since the last call, in order (ASCII only). Use key_down for Backspace and Enter.");

    m.def("play_sound", [](const std::string& name, float volume, float pitch) {
        return audioPlayer().playEffect(name, volume, pitch);
    }, py::arg("name"), py::arg("volume") = 1.0f, py::arg("pitch") = 1.0f,
        "Play the sound sounds/<name>.wav (beside the executable) at a volume from 0 to 1 and a pitch (1 is as recorded). Returns False "
        "if the file cannot be loaded. Under CGENGINE_UNATTENDED nothing is heard, but the call is counted (sound_count).");

    m.def("play_music", [](const std::string& name, float volume) {
        return audioPlayer().playMusic(name, volume);
    }, py::arg("name"), py::arg("volume") = 0.5f,
        "Loop the music sounds/<name>.wav at a volume from 0 to 1, replacing any music that plays. Returns False if it cannot be opened.");

    m.def("stop_music", []() { audioPlayer().stopMusic(); }, "Stop the music.");

    m.def("sound_count", []() { return audioPlayer().effectCount(); },
        "How many sound effects the game has asked for since it started, whether or not they were heard.");

    m.def("set_fullscreen", [](bool on) {
        if (!screen) throw std::runtime_error("the window is not running yet");
        if (!screen->setFullscreen(on)) return false;
        if (renderer && renderer->getCurrentCamera()) {
            const sf::Vector2u size = screen->getWindow()->getSize();
            renderer->getCurrentCamera()->setAspect(static_cast<float>(size.x) / static_cast<float>(size.y));
        }
        return true;
    }, py::arg("on"), "Full screen (a borderless window the size of the desktop) or windowed. False if not supported here.");

    m.def("is_fullscreen", []() { return screen != nullptr && screen->isFullscreen(); }, "Whether the window is full screen.");

    m.def("set_camera_planes", [](float nearDistance, float farDistance) {
        if (!renderer) throw std::runtime_error("the renderer is not running yet");
        renderer->getCurrentCamera()->setPlanes(nearDistance, farDistance);
    }, py::arg("near"), py::arg("far"), "Set the camera's clipping distances in blocks. Nothing past far is drawn.");

    m.def("set_environment", [](std::optional<sf::Vector3f> sky, std::optional<sf::Vector3f> fog, std::optional<float> fogStart,
        std::optional<float> fogEnd, std::optional<sf::Vector3f> zenith, std::optional<sf::Vector3f> sunDirection,
        std::optional<sf::Vector3f> sunColour, std::optional<float> sunSize, std::optional<bool> shadows,
        std::optional<float> shadowRadius, std::optional<sf::Vector3f> moonDirection, std::optional<sf::Vector3f> moonColour,
        std::optional<float> moonSize, std::optional<float> moonLit, std::optional<sf::Vector3f> moonLitAxis) {
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
        if (shadows) environment.shadows = *shadows;
        if (shadowRadius) environment.shadowRadius = *shadowRadius;
        if (moonDirection) environment.moonDirection = *moonDirection;
        if (moonColour) environment.moonColour = *moonColour;
        if (moonSize) environment.moonSize = *moonSize;
        if (moonLit) environment.moonLit = *moonLit;
        if (moonLitAxis) environment.moonLitAxis = *moonLitAxis;
    }, py::arg("sky") = py::none(), py::arg("fog") = py::none(), py::arg("fog_start") = py::none(), py::arg("fog_end") = py::none(),
    py::arg("zenith") = py::none(), py::arg("sun_direction") = py::none(), py::arg("sun_colour") = py::none(),
    py::arg("sun_size") = py::none(), py::arg("shadows") = py::none(), py::arg("shadow_radius") = py::none(),
    py::arg("moon_direction") = py::none(), py::arg("moon_colour") = py::none(), py::arg("moon_size") = py::none(),
    py::arg("moon_lit") = py::none(), py::arg("moon_lit_axis") = py::none(),
    "The sky and fog. sky is the colour at the horizon, zenith the colour straight up (left out, the sky is one colour). fog fades from fog_start to fog_end blocks away; the fog is off while fog_end is not past fog_start. sun_direction points towards the sun, sun_colour draws its disc (black hides it) and sun_size is its angular radius in degrees. shadows turns on the sun's shadows, from a map that covers shadow_radius blocks around the camera. Arguments left out keep their value.");

    m.def("set_light", [](const std::string& name, std::optional<sf::Vector3f> direction, std::optional<sf::Vector3f> colour,
        std::optional<float> brightness, std::optional<float> ambiance, std::optional<sf::Vector3f> position) {
        Light* light = assets.get<Light>(name);
        if (!light) throw std::runtime_error("no light named '" + name + "'");
        // Renderer copies the parameters into the light buffer every frame, so the change applies at once.
        if (direction) light->parameters.lightDirection = *direction;
        if (colour) light->parameters.colorIntensities = *colour;
        if (brightness) light->parameters.brightness = *brightness;
        if (ambiance) light->parameters.ambiance = *ambiance;
        // A point light's position (its w stays 1); a held torch moves its light every frame.
        if (position) light->position = glm::vec4(position->x, position->y, position->z, light->position.w);
    }, py::arg("name"), py::arg("direction") = py::none(), py::arg("colour") = py::none(), py::arg("brightness") = py::none(),
    py::arg("ambiance") = py::none(), py::arg("position") = py::none(),
    "Change a light made by the scene: direction (where the light travels, for a directional light), colour, brightness, ambiance and position (for a point light). Arguments left out keep their value.");

    // Text drawn over the scene, in window pixels from the top-left corner: a label for the game's HUD.
    m.def("create_text_body", [](const std::string& name, const std::string& text, const std::string& fontName,
        sf::Vector2f position, unsigned int size, sf::Color colour, sf::Color outlineColour, float outlineThickness) {
        FontResource* resource = assets.get<FontResource>(fontName);
        if (!resource || !resource->getFont()) throw std::runtime_error("no font named '" + fontName + "' (load_font it first)");
        sf::Text label(*resource->getFont(), text, size);
        label.setFillColor(colour);
        // An outline keeps a label readable over any background; 0 draws the plain text.
        label.setOutlineColor(outlineColour);
        label.setOutlineThickness(outlineThickness);
        label.setPosition(position);
        // The asset name is a cache key, so it carries a prefix; the body's own name is what findBodyByName finds.
        auto body = assets.create<Body>("text:" + name, std::move(label));
        if (!body.has_value()) throw std::runtime_error("could not create the text body '" + name + "'");
        body.value().second->setName(name);
        // Labels are drawn after the meshes: the renderer draws bodies in z order, and meshes are at 0.
        body.value().second->zOrder = 1000;
        return body.value().second;
    }, py::arg("name"), py::arg("text"), py::arg("font"), py::arg("position"), py::arg("size") = 18,
       py::arg("colour") = sf::Color::White, py::arg("outline_colour") = sf::Color::Black, py::arg("outline_thickness") = 0.0f,
       "Make a text label at position (window pixels from the top-left) drawn over the scene. Use a font loaded with load_font. "
       "outline_thickness (pixels) draws an outline in outline_colour, which keeps the text readable over any background.");

    m.def("set_text_body", [](const std::string& name, const std::string& text, std::optional<sf::Vector2f> position,
                              std::optional<sf::Color> colour) {
        Body* body = findBodyByName(name);
        sf::Text* label = body ? body->get<sf::Text*>() : nullptr;
        if (!label) throw std::runtime_error("no text body named '" + name + "'");
        label->setString(text);
        if (position) label->setPosition(*position);
        if (colour) label->setFillColor(*colour);
    }, py::arg("name"), py::arg("text"), py::arg("position") = py::none(), py::arg("colour") = py::none(),
       "Change the text of a label made by create_text_body, and optionally its position (window pixels) and colour.");

    // A filled rectangle over the scene, in window pixels from the top-left corner: the HUD's boxes, slots and swatches. It is an
    // overlay body, so it is drawn over the meshes; its z order puts it under the labels (which are 1000).
    m.def("create_rect_body", [](const std::string& name, sf::Vector2f position, sf::Vector2f size, sf::Color fill,
        sf::Color outlineColour, float outlineThickness, int zOrder) {
        sf::RectangleShape rect(size);
        rect.setPosition(position);
        rect.setFillColor(fill);
        rect.setOutlineColor(outlineColour);
        rect.setOutlineThickness(outlineThickness);
        auto body = assets.create<Body>("rect:" + name, std::move(rect));
        if (!body.has_value()) throw std::runtime_error("could not create the rectangle body '" + name + "'");
        body.value().second->setName(name);
        body.value().second->zOrder = zOrder;
        body.value().second->overlay = true;
        return body.value().second;
    }, py::arg("name"), py::arg("position"), py::arg("size"), py::arg("fill") = sf::Color::White,
       py::arg("outline_colour") = sf::Color::Black, py::arg("outline_thickness") = 0.0f, py::arg("z_order") = 999,
       "Make a rectangle at position with size (window pixels, from the top-left corner), filled with fill (RGBA) and outlined "
       "in outline_colour. A rectangle with alpha 0 fill and no outline is not seen.");

    m.def("set_rect_body", [](const std::string& name, sf::Vector2f position, sf::Vector2f size, sf::Color fill,
        sf::Color outlineColour, float outlineThickness) {
        Body* body = findBodyByName(name);
        sf::RectangleShape* rect = body ? body->get<sf::RectangleShape*>() : nullptr;
        if (!rect) throw std::runtime_error("no rectangle body named '" + name + "'");
        rect->setPosition(position);
        rect->setSize(size);
        rect->setFillColor(fill);
        rect->setOutlineColor(outlineColour);
        rect->setOutlineThickness(outlineThickness);
    }, py::arg("name"), py::arg("position"), py::arg("size"), py::arg("fill"), py::arg("outline_colour"),
       py::arg("outline_thickness") = 0.0f, "Change a rectangle made by create_rect_body: position, size, fill and outline.");

    m.def("window_size", []() {
        const sf::Vector2u size = gameWindow().getSize();
        return sf::Vector2f(static_cast<float>(size.x), static_cast<float>(size.y));
    }, "The window's size in pixels.");

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
