#pragma once

#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace CGEngine {
    /// <summary>
    /// Outcome of loading an animation clip. On failure, ok is false, error says why, and nothing is registered.
    /// </summary>
    struct ClipLoadResult {
        bool ok = false;
        std::string error;
        std::string name;
    };

    /// <summary>
    /// Builds an animation clip from a JSON description (format version 1) and registers it with its model.
    /// See docs/ai/clip-format.md.
    /// </summary>
    class ClipLoader {
    public:
        static ClipLoadResult loadFile(const std::filesystem::path& path);
        static ClipLoadResult loadJson(const nlohmann::json& clip);
    };
}
