#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <optional>
#include <utility>
#include <filesystem>

#include "../Core/Engine/Engine.h"
#include "../Core/AssetManager/AssetManager.h"
//AssetManager types
#include "../Core/Types/Types.h"
#include "../Core/Body/Body.h"
#include "../Core/Material/Material.h"
#include "../Core/Shader/Program.h"
#include "../Core/Mesh/Model.h"
#include "../Core/Animation/Animation.h"
#include "../Core/Skeleton/Skeleton.h"

namespace py = pybind11;

// Helper function to handle the optional<pair<id_t, T*>> result
template <typename T>
py::object handle_asset_result(const std::optional<std::pair<CGEngine::id_t, T*>>& result) {
    if (result.has_value()) {
        // Need to cast T* to py::object. Assumes T is bound.
         // Using reference policy as AssetManager still owns the object.
        return py::cast(std::make_pair(result.value().first, py::cast(result.value().second, py::return_value_policy::reference)));
    }
    else {
        return py::none();
    }
}


void bindAssetManager(py::module_& m) {
    // Expose AssetManager functionality via module-level functions
    // accessing the global 'CGEngine::assets' instance.

    // --- getId<T>(name) ---
    m.def("get_body_id", [](const std::string& name) { return CGEngine::assets.getId<CGEngine::Body>(name); },
        py::arg("resource_name"), "Get the ID of a Body resource by name.");
    m.def("get_material_id", [](const std::string& name) { return CGEngine::assets.getId<CGEngine::Material>(name); },
        py::arg("resource_name"), "Get the ID of a Material resource by name.");
    m.def("get_program_id", [](const std::string& name) { return CGEngine::assets.getId<CGEngine::Program>(name); },
        py::arg("resource_name"), "Get the ID of a Program resource by name.");
    m.def("get_texture_id", [](const std::string& name) { return CGEngine::assets.getId<CGEngine::TextureResource>(name); },
        py::arg("resource_name"), "Get the ID of a TextureResource by name.");
    m.def("get_font_id", [](const std::string& name) { return CGEngine::assets.getId<CGEngine::FontResource>(name); },
        py::arg("resource_name"), "Get the ID of a FontResource by name.");
    m.def("get_model_id", [](const std::string& name) { return CGEngine::assets.getId<CGEngine::Model>(name); },
        py::arg("resource_name"), "Get the ID of a Model resource by name.");
    // Add get_animation_id, get_skeleton_id etc. if needed

    // --- get<T>(id) ---
    m.def("get_body", [](CGEngine::id_t id) { return CGEngine::assets.get<CGEngine::Body>(id); },
        py::arg("resource_id"), py::return_value_policy::reference, "Get a Body resource by ID.");
    m.def("get_material", [](CGEngine::id_t id) { return CGEngine::assets.get<CGEngine::Material>(id); },
        py::arg("resource_id"), py::return_value_policy::reference, "Get a Material resource by ID.");
    m.def("get_program", [](CGEngine::id_t id) { return CGEngine::assets.get<CGEngine::Program>(id); },
        py::arg("resource_id"), py::return_value_policy::reference, "Get a Program resource by ID.");
    m.def("get_texture", [](CGEngine::id_t id) { return CGEngine::assets.get<CGEngine::TextureResource>(id); },
        py::arg("resource_id"), py::return_value_policy::reference, "Get a TextureResource by ID.");
    m.def("get_font", [](CGEngine::id_t id) { return CGEngine::assets.get<CGEngine::FontResource>(id); },
        py::arg("resource_id"), py::return_value_policy::reference, "Get a FontResource by ID.");
    m.def("get_model", [](CGEngine::id_t id) { return CGEngine::assets.get<CGEngine::Model>(id); },
        py::arg("resource_id"), py::return_value_policy::reference, "Get a Model resource by ID.");
    // Add get_animation, get_skeleton etc. if needed

   // --- get<T>(name) ---
    m.def("get_body_by_name", [](const std::string& name) { return CGEngine::assets.get<CGEngine::Body>(name); },
        py::arg("resource_name"), py::return_value_policy::reference, "Get a Body resource by name.");
    m.def("get_material_by_name", [](const std::string& name) { return CGEngine::assets.get<CGEngine::Material>(name); },
        py::arg("resource_name"), py::return_value_policy::reference, "Get a Material resource by name.");
    m.def("get_program_by_name", [](const std::string& name) { return CGEngine::assets.get<CGEngine::Program>(name); },
        py::arg("resource_name"), py::return_value_policy::reference, "Get a Program resource by name.");
    m.def("get_texture_by_name", [](const std::string& name) { return CGEngine::assets.get<CGEngine::TextureResource>(name); },
        py::arg("resource_name"), py::return_value_policy::reference, "Get a TextureResource by name.");
    m.def("get_font_by_name", [](const std::string& name) { return CGEngine::assets.get<CGEngine::FontResource>(name); },
        py::arg("resource_name"), py::return_value_policy::reference, "Get a FontResource by name.");
    m.def("get_model_by_name", [](const std::string& name) { return CGEngine::assets.get<CGEngine::Model>(name); },
        py::arg("resource_name"), py::return_value_policy::reference, "Get a Model resource by name.");

    // --- load<T>(path, name) --- (Returns Optional[Tuple[int, T]])
    m.def("load_texture", [](const std::filesystem::path& path, const std::string& name = "") {
        auto result = CGEngine::assets.load<CGEngine::TextureResource>(path, name);
        return handle_asset_result(result); // Use helper
        }, py::arg("resource_path"), py::arg("resource_name") = "", "Load a TextureResource from file. Returns (id, resource) tuple or None.");

    m.def("load_font", [](const std::filesystem::path& path, const std::string& name = "") {
        auto result = CGEngine::assets.load<CGEngine::FontResource>(path, name);
        return handle_asset_result(result); // Use helper
        }, py::arg("resource_path"), py::arg("resource_name") = "", "Load a FontResource from file. Returns (id, resource) tuple or None.");

    m.def("load_model", [](const std::filesystem::path& path, const std::string& name = "") {
        auto result = CGEngine::assets.load<CGEngine::Model>(path, name);
        return handle_asset_result(result); // Use helper
        }, py::arg("resource_path"), py::arg("resource_name") = "", "Load a Model from file. Returns (id, resource) tuple or None.");
    // Add load_vertex_shader, load_fragment_shader if needed


    // --- create<T>(name, ...Args) --- (Returns Optional[Tuple[int, T]])
    // Need specific versions for creatable types

    // Create Program
    m.def("create_program", [](const std::string& name, const std::string& vert_path, const std::string& frag_path) {
        // Assuming Program constructor takes paths or ShaderProgramPath
        auto result = CGEngine::assets.create<CGEngine::Program>(name, vert_path, frag_path, name); // Pass name twice? Check constructor
        return handle_asset_result(result);
        }, py::arg("resource_name"), py::arg("vertex_shader_path"), py::arg("fragment_shader_path"),
            "Create a shader Program resource. Returns (id, resource) tuple or None.");

    // Create Material
    m.def("create_material", [](const std::string& name, const CGEngine::SurfaceParameters& params, CGEngine::Program* program) {
        if (!program) {
            throw py::value_error("Program cannot be null for material creation");
        }
        auto result = CGEngine::assets.create<CGEngine::Material>(name, params, program);
        return handle_asset_result(result);
        }, py::arg("resource_name"), py::arg("surface_params"), py::arg("program"),
            "Create a Material resource. Returns (id, resource) tuple or None.");

    // Create Body (Very complex due to template constructor and entity types)
    // Would need specific functions like 'create_body_with_mesh', etc. - Deferred for now.


    // --- Defaults ---
    m.def("get_default_texture_name", []() { return CGEngine::assets.defaultTextureName; },
        "Get the default name used for texture resources.");
    m.def("get_default_program_name", []() { return CGEngine::assets.defaultProgramName; },
        "Get the default name used for program resources.");
    m.def("get_default_material_name", []() { return CGEngine::assets.defaultMaterialName; },
        "Get the default name used for material resources.");
    m.def("get_default_font_name", []() { return CGEngine::assets.defaultFontName; },
        "Get the default name used for font resources.");

    // --- Default IDs ---
    m.def("get_default_texture_id", []() { return CGEngine::assets.getDefaultId<CGEngine::TextureResource>(); });
    m.def("get_default_program_id", []() { return CGEngine::assets.getDefaultId<CGEngine::Program>(); });
    m.def("get_default_material_id", []() { return CGEngine::assets.getDefaultId<CGEngine::Material>(); });
    m.def("get_default_font_id", []() { return CGEngine::assets.getDefaultId<CGEngine::FontResource>(); });

    //Body creators
    m.def("create_body_with_name", [](const std::string& name) {
        auto result = CGEngine::assets.create<CGEngine::Body>(name);
        if (result.has_value()) {
            return result.value().second; // Return the Body pointer
        }
        return (CGEngine::Body*)nullptr; // Handle the case where creation failed
        }, "Create a Body with a name", py::arg("name"));
    m.def("create_body_as_root", []() {
        auto result = CGEngine::assets.create<CGEngine::Body>("Root", true);
        if (result.has_value()) {
            return result.value().second; // Return the Body pointer
        }
        return (CGEngine::Body*)nullptr; // Handle the case where creation failed
        }, "Create the root Body");
    m.def("create_body_with_mesh", [](const std::string& name, CGEngine::Mesh* mesh) {
        // Assuming Body can be created with a Mesh pointer. Adjust as needed.
        return CGEngine::assets.create<CGEngine::Body>(name, mesh);
        }, "Create a Body with a mesh", py::arg("name"), py::arg("mesh_ptr"));
    //Defer - create_body_with_shape
    //Defer - create_body_with_text
    //Defer - create_body_with_sprite
}