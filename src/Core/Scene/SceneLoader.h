#pragma once
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace CGEngine {
    /// <summary>
    /// Outcome of loading a scene description. On failure, ok is false and error says why.
    /// Counts report how many assets were created. Nothing is rolled back on partial failure.
    /// </summary>
    struct SceneLoadResult {
        bool ok = false;
        std::string error;
        size_t materials = 0;
        size_t lights = 0;
        size_t bodies = 0;
    };

    /// <summary>
    /// Builds materials, lights and model bodies from a JSON scene description (format version 1).
    /// See docs/ai/scene-format.md for the schema.
    /// </summary>
    class SceneLoader {
    public:
        static SceneLoadResult loadFile(const std::filesystem::path& path);
        static SceneLoadResult loadJson(const nlohmann::json& scene);
    };
}
