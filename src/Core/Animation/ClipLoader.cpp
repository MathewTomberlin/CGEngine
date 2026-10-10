#include "ClipLoader.h"
#include "../Engine/Engine.h"
#include "../Mesh/Model.h"
#include "../Mesh/Bone.h"
#include "../Animation/Animation.h"
#include "../Skeleton/Skeleton.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <gtx/matrix_decompose.hpp>

using json = nlohmann::json;

namespace CGEngine {
    namespace {
        glm::vec3 readVec3(const json& value, const std::string& context) {
            if (!value.is_array() || value.size() != 3 || !value[0].is_number() || !value[1].is_number() || !value[2].is_number()) {
                throw std::invalid_argument(context + " must be [x, y, z] numbers");
            }
            return glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
        }

        // Finds the node with the given name in the hierarchy, or nullptr.
        const NodeData* findNode(const NodeData& node, const std::string& name) {
            if (node.name == name) return &node;
            for (const NodeData& child : node.children) {
                if (const NodeData* found = findNode(child, name)) return found;
            }
            return nullptr;
        }

        // Rest pose of a bone: its local transform in the model's hierarchy, split into parts.
        struct RestPose { glm::vec3 position; glm::quat rotation; glm::vec3 scale; };
        RestPose restPoseOf(const NodeData* node) {
            RestPose rest { glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f) };
            if (!node) return rest;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(node->transformation, rest.scale, rest.rotation, rest.position, skew, perspective);
            return rest;
        }

        // Reads the keyframe times of one property. Times are seconds, strictly increasing, and inside the clip.
        std::vector<float> readTimes(const json& keys, const char* property, float duration, const std::string& context) {
            if (!keys.is_array() || keys.empty()) {
                throw std::invalid_argument(context + ": '" + property + "' must be a non-empty array of keyframes");
            }
            std::vector<float> times;
            float previous = -1.0f;
            for (const json& key : keys) {
                if (!key.contains("t") || !key.at("t").is_number()) {
                    throw std::invalid_argument(context + ": each '" + property + "' keyframe needs a numeric \"t\" in seconds");
                }
                float t = key.at("t").get<float>();
                if (t < 0.0f || t > duration) {
                    throw std::invalid_argument(context + ": '" + property + "' keyframe at t=" + std::to_string(t) + " is outside the clip (0.." + std::to_string(duration) + ")");
                }
                if (t <= previous) {
                    throw std::invalid_argument(context + ": '" + property + "' keyframe times must strictly increase");
                }
                previous = t;
                times.push_back(t);
            }
            return times;
        }
    }

    ClipLoadResult ClipLoader::loadFile(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file) {
            ClipLoadResult result;
            result.error = "cannot open clip file '" + path.string() + "'";
            return result;
        }
        json clip;
        try {
            clip = json::parse(file);
        } catch (const json::parse_error& e) {
            ClipLoadResult result;
            result.error = std::string("invalid JSON in clip '") + path.string() + "': " + e.what();
            return result;
        }
        return loadJson(clip);
    }

    ClipLoadResult ClipLoader::loadJson(const json& clip) {
        ClipLoadResult result;
        try {
            if (!clip.is_object() || clip.value("version", 0) != 1) {
                throw std::invalid_argument("clip must be an object with \"version\": 1");
            }
            std::string name = clip.at("name").get<std::string>();
            std::string context = "clip '" + name + "'";
            if (name.empty()) throw std::invalid_argument("clip name must not be empty");
            if (assets.getId<Animation>(name).has_value()) {
                throw std::invalid_argument("an animation named '" + name + "' already exists");
            }
            float duration = clip.at("durationSeconds").get<float>();
            if (!(duration > 0.0f)) throw std::invalid_argument(context + ": durationSeconds must be positive");

            // The clip targets a model. Its skeleton defines the bones, and one of its existing clips supplies
            // the node hierarchy, which the animator walks to reach every bone.
            std::string modelPath = clip.at("model").get<std::string>();
            auto modelAsset = assets.load<Model>(modelPath);
            if (!modelAsset.has_value()) throw std::runtime_error(context + ": failed to load model '" + modelPath + "'");
            Model* model = modelAsset.value().second;
            Skeleton* skeleton = model->getSkeleton();
            if (!skeleton) throw std::invalid_argument(context + ": model '" + modelPath + "' is not skeletal");
            if (model->getAnimationNames().empty()) {
                throw std::invalid_argument(context + ": model '" + modelPath + "' has no imported clip to take the bone hierarchy from");
            }
            optional<id_t> hierarchyId = assets.getId<Animation>(model->getAnimationNames().front());
            if (!hierarchyId.has_value()) throw std::runtime_error(context + ": hierarchy clip is missing");
            const NodeData hierarchy = assets.get<Animation>(hierarchyId.value())->getRoot();

            // Validate every channel before creating anything.
            std::vector<Bone> bones;
            for (const json& channel : clip.at("channels")) {
                std::string boneName = channel.at("bone").get<std::string>();
                std::string channelContext = context + ", bone '" + boneName + "'";
                optional<BoneData> boneData = skeleton->getBoneData(boneName);
                if (!boneData.has_value()) {
                    throw std::invalid_argument(channelContext + ": not a bone of model '" + modelPath + "' (see list_animations)");
                }
                if (std::any_of(bones.begin(), bones.end(), [&](const Bone& b) { return b.getBoneName() == boneName; })) {
                    throw std::invalid_argument(channelContext + ": listed more than once");
                }

                // Keys are relative to the bone's rest pose, which comes from the model's hierarchy:
                //   position: offset added to the rest position
                //   rotation: Euler degrees, applied on top of the rest rotation
                //   scale:    absolute scale
                // A property with no keys holds its rest value, so every bone always has at least one key of each kind.
                RestPose rest = restPoseOf(findNode(hierarchy, boneName));

                std::vector<KeyPosition> positions;
                if (channel.contains("position")) {
                    std::vector<float> times = readTimes(channel.at("position"), "position", duration, channelContext);
                    for (size_t i = 0; i < times.size(); ++i) {
                        glm::vec3 offset = readVec3(channel.at("position")[i].at("v"), channelContext + " position value");
                        positions.push_back({ rest.position + offset, times[i] });
                    }
                } else {
                    positions.push_back({ rest.position, 0.0f });
                }

                std::vector<KeyRotation> rotations;
                if (channel.contains("rotation")) {
                    std::vector<float> times = readTimes(channel.at("rotation"), "rotation", duration, channelContext);
                    for (size_t i = 0; i < times.size(); ++i) {
                        glm::vec3 degrees = readVec3(channel.at("rotation")[i].at("euler"), channelContext + " rotation euler");
                        rotations.push_back({ rest.rotation * glm::quat(glm::radians(degrees)), times[i] });
                    }
                } else {
                    rotations.push_back({ rest.rotation, 0.0f });
                }

                std::vector<KeyScale> scales;
                if (channel.contains("scale")) {
                    std::vector<float> times = readTimes(channel.at("scale"), "scale", duration, channelContext);
                    for (size_t i = 0; i < times.size(); ++i) {
                        scales.push_back({ readVec3(channel.at("scale")[i].at("v"), channelContext + " scale value"), times[i] });
                    }
                } else {
                    scales.push_back({ rest.scale, 0.0f });
                }

                bones.emplace_back(boneName, boneData->id, positions, rotations, scales);
            }

            // Everything is valid: create and register the clip.
            auto created = assets.create<Animation>(name);
            if (!created.has_value()) throw std::runtime_error(context + ": failed to create the animation");
            Animation* animation = created.value().second;
            animation->setName(name);
            animation->duration = duration;
            animation->ticksPerSecond = 1.0f; // clip times are in seconds
            animation->bones = std::move(bones);
            animation->root = hierarchy;
            model->addAnimationName(name);

            result.ok = true;
            result.name = name;
        } catch (const json::exception& e) {
            result.error = std::string("clip JSON error: ") + e.what();
        } catch (const std::exception& e) {
            result.error = e.what();
        }
        return result;
    }
}
