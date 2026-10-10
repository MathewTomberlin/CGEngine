#include "ControlChannel.h"
#include "../Engine/Engine.h"
#include "../Mesh/Mesh.h"
#include "../Light/Light.h"
#include "../Material/Material.h"
#include "../Body/Body.h"
#include "../Mesh/Model.h"
#include "../Animation/Animator.h"
#include "../Animation/Animation.h"
#include "../Animation/ClipLoader.h"
#include "../Skeleton/Skeleton.h"
#include "../Scene/SceneLoader.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace CGEngine {
    namespace {
        constexpr unsigned long long pollIntervalFrames = 10;
        constexpr size_t maxCommandsPerPoll = 16;

        unsigned long long frames = 0;
        unsigned long long framesSincePoll = 0;

        fs::path inboxDir() { return fs::path("cg_control") / "inbox"; }
        fs::path outboxDir() { return fs::path("cg_control") / "outbox"; }

        json vec3(const Vector3f& v) { return json::array({ v.x, v.y, v.z }); }

        Vector3f readVec3(const json& value, const char* key) {
            if (!value.is_array() || value.size() != 3) {
                throw std::invalid_argument(std::string("'") + key + "' must be [x, y, z]");
            }
            return Vector3f(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
        }

        Body* findBodyByName(const std::string& name);

        // The model a named body belongs to: the first mesh in its subtree that was imported from a model.
        Model* findModelFor(Body* body) {
            Model* found = nullptr;
            body->apply([&found](Body* b) {
                if (found) return;
                if (Mesh* mesh = b->get<Mesh*>()) {
                    if (mesh->getModelId().has_value()) found = assets.get<Model>(mesh->getModelId().value());
                }
            });
            return found;
        }

        Animator* requireAnimator(const std::string& name, Model*& model) {
            Body* body = findBodyByName(name);
            if (!body) throw std::runtime_error("no body named '" + name + "'");
            model = findModelFor(body);
            if (!model) throw std::runtime_error("body '" + name + "' is not part of an imported model");
            Animator* animator = model->getAnimator();
            if (!animator) throw std::runtime_error("model of body '" + name + "' has no animation clips");
            return animator;
        }

        double clipDurationSeconds(const std::string& clip) {
            optional<id_t> id = assets.getId<Animation>(clip);
            if (!id.has_value()) return 0.0;
            Animation* animation = assets.get<Animation>(id.value());
            float ticksPerSecond = animation->getTicksPerSecond() > 0 ? animation->getTicksPerSecond() : 24.0f;
            return animation->getDuration() / ticksPerSecond;
        }

        Body* findBodyByName(const std::string& name) {
            Body* found = nullptr;
            for (Body* body : assets.getAllResources<Body>()) {
                if (body->getName() == name) {
                    found = body;
                    break;
                }
            }
            return found;
        }

        json describeScene() {
            // Model sub-parts are unnamed; report named bodies with a child count instead of every part.
            std::map<id_t, size_t> childCount;
            for (Body* body : assets.getAllResources<Body>()) {
                if (body->getParentId().has_value()) childCount[body->getParentId().value()]++;
            }
            json bodies = json::array();
            for (Body* body : assets.getAllResources<Body>()) {
                if (body->getName().empty()) continue;
                json entry = { {"id", body->getId().value_or(0)}, {"name", body->getName()} };
                if (body->getParentId().has_value()) entry["parent"] = body->getParentId().value();
                entry["children"] = childCount[body->getId().value_or(0)];
                if (Mesh* mesh = body->get<Mesh*>()) {
                    Transformation3D t = mesh->getTransformation();
                    entry["position"] = vec3(t.position);
                    entry["rotation"] = vec3(t.rotation);
                    entry["scale"] = vec3(t.scale);
                }
                bodies.push_back(entry);
            }

            json lights = json::array();
            for (Light* light : assets.getAllResources<Light>()) {
                json entry = { {"name", assets.getName<Light>(light->getId().value_or(0)).value_or("")}, {"id", light->getId().value_or(0)} };
                entry["position"] = vec3(Vector3f(light->position.x, light->position.y, light->position.z));
                entry["brightness"] = light->parameters.brightness;
                lights.push_back(entry);
            }

            json materials = json::array();
            for (Material* material : assets.getAllResources<Material>()) {
                materials.push_back({ {"name", assets.getName<Material>(material->getId().value_or(0)).value_or("")}, {"id", material->getId().value_or(0)} });
            }

            return { {"bodies", bodies}, {"lights", lights}, {"materials", materials} };
        }

        json runCommand(const std::string& command, const json& params) {
            if (command == "load_scene") {
                SceneLoadResult scene = SceneLoader::loadFile(params.at("path").get<std::string>());
                if (!scene.ok) throw std::runtime_error(scene.error);
                return { {"bodies", scene.bodies}, {"lights", scene.lights}, {"materials", scene.materials} };
            }
            if (command == "describe_scene") {
                return describeScene();
            }
            if (command == "set_transform") {
                std::string name = params.at("name").get<std::string>();
                Body* body = findBodyByName(name);
                if (!body) throw std::runtime_error("no body named '" + name + "'");
                Mesh* mesh = body->get<Mesh*>();
                if (!mesh) throw std::runtime_error("body '" + name + "' has no mesh to transform");
                if (params.contains("position")) mesh->setPosition(readVec3(params["position"], "position"));
                if (params.contains("rotation")) mesh->setRotation(readVec3(params["rotation"], "rotation"));
                if (params.contains("scale")) mesh->setScale(readVec3(params["scale"], "scale"));
                Transformation3D t = mesh->getTransformation();
                return { {"name", name}, {"position", vec3(t.position)}, {"rotation", vec3(t.rotation)}, {"scale", vec3(t.scale)} };
            }
            if (command == "screenshot") {
                std::string path = params.at("path").get<std::string>();
                renderer->requestScreenshot(path);
                return { {"queued", true}, {"path", path}, {"note", "written after the next rendered frame"} };
            }
            if (command == "set_material") {
                std::string name = params.at("name").get<std::string>();
                std::string materialName = params.at("material").get<std::string>();
                Body* body = findBodyByName(name);
                if (!body) throw std::runtime_error("no body named '" + name + "'");
                optional<id_t> materialId = assets.getId<Material>(materialName);
                if (!materialId.has_value()) throw std::runtime_error("no material named '" + materialName + "'");
                size_t meshes = 0;
                auto assignMaterial = [&](Body* b) {
                    if (Mesh* mesh = b->get<Mesh*>()) {
                        mesh->setMaterials({ materialId.value() });
                        meshes++;
                    }
                };
                // recursive (default true) also applies to the sub-parts of a model instance.
                if (params.value("recursive", true)) body->apply(assignMaterial);
                else assignMaterial(body);
                return { {"name", name}, {"material", materialName}, {"meshesUpdated", meshes} };
            }
            if (command == "remove_body") {
                std::string name = params.at("name").get<std::string>();
                Body* body = findBodyByName(name);
                if (!body) throw std::runtime_error("no body named '" + name + "'");
                if (body == world->getRoot()) throw std::runtime_error("the world root cannot be removed");
                // children: "terminate" (default) removes the subtree, "orphan" moves children to the root, "inherit" to the parent.
                std::string children = params.value("children", std::string("terminate"));
                ChildrenTermination termination = ChildrenTermination::Terminate;
                if (children == "orphan") termination = ChildrenTermination::Orphan;
                else if (children == "inherit") termination = ChildrenTermination::Inherit;
                else if (children != "terminate") throw std::runtime_error("children must be terminate, orphan or inherit");
                body->deleteBody(termination);
                return { {"removed", name}, {"children", children} };
            }
            if (command == "load_clip") {
                ClipLoadResult clip = ClipLoader::loadFile(params.at("path").get<std::string>());
                if (!clip.ok) throw std::runtime_error(clip.error);
                return { {"name", clip.name} };
            }
            if (command == "list_animations") {
                Model* model = nullptr;
                Animator* animator = requireAnimator(params.at("name").get<std::string>(), model);
                json clips = json::array();
                for (const std::string& clip : model->getAnimationNames()) {
                    clips.push_back({ {"name", clip}, {"durationSeconds", clipDurationSeconds(clip)} });
                }
                json bones = json::array();
                if (Skeleton* skeleton = model->getSkeleton()) {
                    for (const std::string& bone : skeleton->getBoneNames()) bones.push_back(bone);
                }
                return { {"bones", bones}, {"current", animator->getCurrentAnimationName()}, {"timeSeconds", animator->getTimeSeconds()},
                         {"durationSeconds", animator->getDurationSeconds()}, {"paused", animator->isPaused()},
                         {"speed", animator->getSpeed()}, {"looping", animator->isLooping()}, {"animations", clips} };
            }
            if (command == "play_animation") {
                std::string name = params.at("name").get<std::string>();
                std::string clip = params.at("animation").get<std::string>();
                Model* model = nullptr;
                Animator* animator = requireAnimator(name, model);
                const auto& clips = model->getAnimationNames();
                if (std::find(clips.begin(), clips.end(), clip) == clips.end()) {
                    throw std::runtime_error("model of '" + name + "' has no animation named '" + clip + "'");
                }
                animator->playAnimation(clip);
                animator->setSpeed(params.value("speed", 1.0f));
                animator->setLooping(params.value("looping", true));
                animator->setPaused(false);
                return { {"name", name}, {"animation", clip}, {"speed", animator->getSpeed()}, {"looping", animator->isLooping()} };
            }
            if (command == "pause_animation") {
                std::string name = params.at("name").get<std::string>();
                Model* model = nullptr;
                Animator* animator = requireAnimator(name, model);
                animator->setPaused(params.value("paused", true));
                return { {"name", name}, {"paused", animator->isPaused()} };
            }
            if (command == "get_stats") {
                return {
                    {"frames", frames},
                    {"frameSeconds", time.getDeltaSec()},
                    {"drawCalls", renderer->getLastFrameDrawCalls()},
                    {"bodies", assets.getResourceCount<Body>()},
                    {"lights", assets.getResourceCount<Light>()},
                    {"materials", assets.getResourceCount<Material>()}
                };
            }
            throw std::runtime_error("unknown command '" + command + "'");
        }

        void writeResult(const fs::path& commandFile, const json& response) {
            fs::path name = commandFile.stem();
            fs::path temp = outboxDir() / (name.string() + ".result.json.tmp");
            fs::path target = outboxDir() / (name.string() + ".result.json");
            {
                std::ofstream out(temp);
                out << response.dump(2) << "\n";
            }
            fs::rename(temp, target);
        }

        void processCommandFile(const fs::path& file) {
            json response;
            try {
                std::ifstream in(file);
                json command = json::parse(in);
                std::string name = command.at("command").get<std::string>();
                json params = command.value("params", json::object());
                response = { {"ok", true}, {"result", runCommand(name, params)} };
            } catch (const std::exception& e) {
                response = { {"ok", false}, {"error", e.what()} };
            }
            writeResult(file, response);
            std::error_code ec;
            fs::remove(file, ec);
        }
    }

    void ControlChannel::initialize() {
        std::error_code ec;
        fs::create_directories(inboxDir(), ec);
        fs::create_directories(outboxDir(), ec);
    }

    unsigned long long ControlChannel::frameCount() {
        return frames;
    }

    void ControlChannel::poll() {
        ++frames;
        if (++framesSincePoll < pollIntervalFrames) return;
        framesSincePoll = 0;

        std::error_code ec;
        std::vector<fs::path> commands;
        for (const auto& entry : fs::directory_iterator(inboxDir(), ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
                commands.push_back(entry.path());
            }
        }
        std::sort(commands.begin(), commands.end());
        if (commands.size() > maxCommandsPerPoll) commands.resize(maxCommandsPerPoll);
        for (const fs::path& file : commands) {
            processCommandFile(file);
        }
    }
}
