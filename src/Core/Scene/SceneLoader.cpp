#include "SceneLoader.h"
#include "../Engine/Engine.h"
#include "../../Standard/Models/CommonModels.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace CGEngine {
    namespace {
        Vector3f readVec3(const json& obj, const char* key, Vector3f fallback) {
            if (!obj.contains(key)) return fallback;
            const json& value = obj.at(key);
            if (!value.is_array() || value.size() != 3) {
                throw std::invalid_argument(std::string("'") + key + "' must be an array of 3 numbers");
            }
            return Vector3f(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
        }

        std::string requireString(const json& obj, const char* key, const std::string& context) {
            if (!obj.contains(key) || !obj.at(key).is_string()) {
                throw std::invalid_argument(context + ": '" + key + "' is required and must be a string");
            }
            return obj.at(key).get<std::string>();
        }

        // Cube or plane generated in code. Meshes are cached per size, so bodies can share them.
        void createPrimitiveBody(const json& b, const std::string& name, const Transformation3D& transform, const std::vector<id_t>& overrides) {
            std::string primitive = requireString(b, "primitive", "body '" + name + "'");
            if (primitive != "cube" && primitive != "plane") {
                throw std::invalid_argument("body '" + name + "' has unknown primitive '" + primitive + "' (use \"cube\" or \"plane\")");
            }
            float size = b.value("size", 1.0f);
            std::ostringstream meshName;
            meshName << "primitive:" << primitive << ":" << size;

            auto meshData = assets.create<MeshData>(meshName.str(),
                primitive == "cube" ? getCubeVertices(size) : getPlaneVertices(size, 0.0f, Vector3f(0, 0, 0)),
                primitive == "cube" ? getCubeIndices() : getPlaneIndices());
            if (!meshData.has_value()) {
                throw std::runtime_error("failed to create mesh for primitive body '" + name + "'");
            }

            id_t materialId = overrides.empty() ? assets.getDefaultId<Material>().value_or(0) : overrides.front();
            Mesh mesh(meshData.value().second, transform, { materialId });
            // Bodies are cached by asset name, so the name carries the scene body name to stay unique.
            auto body = assets.create<Body>("body:" + name, mesh);
            if (!body.has_value()) {
                throw std::runtime_error("failed to create body '" + name + "'");
            }
            body.value().second->setName(name);
        }

        std::string nameOf(const json& obj, const std::string& context) {
            return requireString(obj, "name", context);
        }
    }

    SceneLoadResult SceneLoader::loadFile(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file) {
            SceneLoadResult result;
            result.error = "cannot open scene file '" + path.string() + "'";
            return result;
        }
        json scene;
        try {
            scene = json::parse(file);
        } catch (const json::parse_error& e) {
            SceneLoadResult result;
            result.error = std::string("invalid JSON in '") + path.string() + "': " + e.what();
            return result;
        }
        return loadJson(scene);
    }

    SceneLoadResult SceneLoader::loadJson(const json& scene) {
        SceneLoadResult result;
        try {
            if (!scene.is_object() || scene.value("version", 0) != 1) {
                throw std::invalid_argument("scene must be an object with \"version\": 1");
            }

            // Materials first, so bodies can reference them by name.
            if (scene.contains("materials")) {
                for (const json& m : scene.at("materials")) {
                    std::string name = nameOf(m, "material");
                    std::string texture = requireString(m, "diffuseTexture", "material '" + name + "'");
                    float uvScale = m.value("uvScale", 1.0f);
                    auto created = assets.create<Material>(name,
                        SurfaceParameters(SurfaceDomain(texture, uvScale)),
                        assets.get<Program>(assets.defaultProgramName));
                    if (!created.has_value()) {
                        throw std::runtime_error("failed to create material '" + name + "'");
                    }
                    result.materials++;
                }
            }

            if (scene.contains("lights")) {
                for (const json& l : scene.at("lights")) {
                    std::string name = nameOf(l, "light");
                    Vector3f position = readVec3(l, "position", Vector3f(0, 0, 0));
                    bool directional = l.value("directional", false);
                    LightParameters params(
                        l.value("brightness", 5.0f),
                        readVec3(l, "color", Vector3f(1, 1, 1)),
                        l.value("attenuation", 0.005f),
                        l.value("ambiance", 0.001f),
                        l.value("coneAngle", 180.0f),
                        readVec3(l, "direction", Vector3f(0, 0, -1)));
                    auto created = assets.create<Light>(name, position, directional, params);
                    if (!created.has_value()) {
                        throw std::runtime_error("failed to create light '" + name + "'");
                    }
                    result.lights++;
                }
            }

            if (scene.contains("bodies")) {
                for (const json& b : scene.at("bodies")) {
                    std::string name = nameOf(b, "body");
                    Transformation3D transform(
                        readVec3(b, "position", Vector3f(0, 0, 0)),
                        readVec3(b, "rotation", Vector3f(0, 0, 0)),
                        readVec3(b, "scale", Vector3f(1, 1, 1)));

                    // An optional material applies to every mesh of the model.
                    std::vector<id_t> overrides;
                    if (b.contains("material")) {
                        std::string materialName = requireString(b, "material", "body '" + name + "'");
                        optional<id_t> materialId = assets.getId<Material>(materialName);
                        if (!materialId.has_value()) {
                            throw std::invalid_argument("body '" + name + "' references unknown material '" + materialName + "'");
                        }
                        overrides.push_back(materialId.value());
                    }

                    if (b.contains("primitive")) {
                        createPrimitiveBody(b, name, transform, overrides);
                        result.bodies++;
                        continue;
                    }

                    std::string modelPath = requireString(b, "model", "body '" + name + "'");
                    auto model = assets.load<Model>(modelPath);
                    if (!model.has_value()) {
                        throw std::runtime_error("failed to load model '" + modelPath + "' for body '" + name + "'");
                    }
                    optional<id_t> rootId = model.value().second->instantiate(transform, overrides);
                    if (!rootId.has_value()) {
                        throw std::runtime_error("failed to instantiate model '" + modelPath + "' for body '" + name + "'");
                    }
                    assets.get<Body>(rootId.value())->setName(name);
                    result.bodies++;
                }
            }

            result.ok = true;
        } catch (const json::exception& e) {
            result.error = std::string("scene JSON error: ") + e.what();
        } catch (const std::exception& e) {
            result.error = e.what();
        }
        return result;
    }
}
