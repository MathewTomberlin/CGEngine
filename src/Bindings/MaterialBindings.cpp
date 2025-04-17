#include <pybind11/pybind11.h>
#include "../Core/Material/Material.h"
#include "../Core/Shader/Program.h"
#include "../Core/Types/Types.h"

namespace py = pybind11;

// Forward declaration for Program binding if not included directly
// void bindProgram(py::module_&); // Assuming ProgramBindings.cpp exists

void bindMaterial(py::module_& m) {
    // Bind SurfaceDomain struct
    py::class_<CGEngine::SurfaceDomain>(m, "SurfaceDomain")
        .def(py::init<float, sf::Color, std::string, sf::Vector2f, sf::Vector2f, sf::Vector2f>(),
            py::arg("domain_intensity") = 1.0f,
            py::arg("domain_color") = sf::Color::White,
            py::arg("texture_path") = "",
            py::arg("texture_scale") = sf::Vector2f{ 1, 1 },
            py::arg("texture_scroll_speed") = sf::Vector2f{ 0, 0 },
            py::arg("texture_offset") = sf::Vector2f{ 0, 0 },
            "Constructor for SurfaceDomain")
        .def(py::init<std::string, float, sf::Color, sf::Vector2f, sf::Vector2f, sf::Vector2f>(),
            py::arg("texture_path"),
            py::arg("domain_intensity") = 1.0f,
            py::arg("domain_color") = sf::Color::White,
            py::arg("texture_scale") = sf::Vector2f{ 1, 1 },
            py::arg("texture_scroll_speed") = sf::Vector2f{ 0, 0 },
            py::arg("texture_offset") = sf::Vector2f{ 0, 0 },
            "Constructor for SurfaceDomain")
        .def(py::init<std::string, sf::Vector2f, sf::Color, float, sf::Vector2f, sf::Vector2f>(),
            py::arg("texture_path"),
            py::arg("texture_scale"),
            py::arg("domain_color") = sf::Color::White,
            py::arg("domain_intensity") = 1.0f,
            py::arg("texture_scroll_speed") = sf::Vector2f{ 0, 0 },
            py::arg("texture_offset") = sf::Vector2f{ 0, 0 },
            "Constructor for SurfaceDomain")
        .def(py::init<std::string, sf::Color, float, sf::Vector2f, sf::Vector2f, sf::Vector2f>(),
            py::arg("texture_path"),
            py::arg("domain_color"),
            py::arg("domain_intensity") = 1.0f,
            py::arg("texture_scale") = sf::Vector2f{ 1, 1 },
            py::arg("texture_scroll_speed") = sf::Vector2f{ 0, 0 },
            py::arg("texture_offset") = sf::Vector2f{ 0, 0 },
            "Constructor for SurfaceDomain")
        .def_readwrite("texture_path", &CGEngine::SurfaceDomain::texturePath, "Path to the texture used in the domain.")
        .def_readwrite("texture_scale", &CGEngine::SurfaceDomain::textureScale, "Scaling factor for the texture UV coordinates.")
        .def_readwrite("texture_scroll_speed", &CGEngine::SurfaceDomain::textureScrollSpeed, "Scrolling speed of the texture over time.")
        .def_readwrite("texture_offset", &CGEngine::SurfaceDomain::textureOffset, "Offset applied to the texture UV coordinates.")
        .def_readwrite("domain_color", &CGEngine::SurfaceDomain::domainColor, "Color tint applied to the surface domain.")
        .def_readwrite("domain_intensity", &CGEngine::SurfaceDomain::domainIntensity, "Intensity of the surface domain's effect.");


    // Bind SurfaceParameters struct
    py::class_<CGEngine::SurfaceParameters>(m, "SurfaceParameters")
        .def(py::init<std::string, sf::Vector2f, sf::Vector2f, sf::Vector2f, sf::Color,
            std::string, sf::Vector2f, sf::Vector2f, sf::Vector2f, sf::Color,
            float,
            std::string, sf::Vector2f, sf::Vector2f, sf::Vector2f, float,
            bool, float, bool, float, bool>(),
            py::arg("diffuse_texture_path") = "",
            py::arg("diffuse_texture_uv_scale") = sf::Vector2f{ 1, 1 },
            py::arg("diffuse_texture_scroll_speed") = sf::Vector2f{ 0, 0 },
            py::arg("diffuse_texture_offset") = sf::Vector2f{ 0, 0 },
            py::arg("diffuse_color") = sf::Color::White,
            py::arg("specular_texture_path") = "",
            py::arg("specular_texture_uv_scale") = sf::Vector2f{ 1, 1 },
            py::arg("specular_texture_scroll_speed") = sf::Vector2f{ 0, 0 },
            py::arg("specular_texture_offset") = sf::Vector2f{ 0, 0 },
            py::arg("specular_color") = sf::Color::White,
            py::arg("smoothness_factor") = 32.0f,
            py::arg("opacity_texture_path") = "",
            py::arg("opacity_texture_uv_scale") = sf::Vector2f{ 1, 1 },
            py::arg("opacity_texture_scroll_speed") = sf::Vector2f{ 0, 0 },
            py::arg("opacity_texture_offset") = sf::Vector2f{ 0, 0 },
            py::arg("opacity") = 1.0f,
            py::arg("opacity_masked") = true,
            py::arg("alpha_cutoff") = 0.01f,
            py::arg("use_gamma_correction") = false,
            py::arg("gamma") = 2.2f,
            py::arg("use_lighting") = true,
            "Constructor for SurfaceParameters")
        .def(py::init<CGEngine::SurfaceDomain, CGEngine::SurfaceDomain, CGEngine::SurfaceDomain, bool, float, bool, float, bool>(),
            py::arg("diffuse_domain") = CGEngine::SurfaceDomain(),
            py::arg("specular_domain") = CGEngine::SurfaceDomain(32.0f),
            py::arg("opacity_domain") = CGEngine::SurfaceDomain(),
            py::arg("opacity_masked") = true,
            py::arg("alpha_cutoff") = 0.01f,
            py::arg("use_gamma_correction") = false,
            py::arg("gamma") = 2.2f,
            py::arg("use_lighting") = true,
            "Constructor for Surface Parameters using SurfaceDomain")
        .def_readwrite("diffuseTexturePath", &CGEngine::SurfaceParameters::diffuseTexturePath, "Path to the diffuse texture.")
        .def_readwrite("diffuseTextureUVScale", &CGEngine::SurfaceParameters::diffuseTextureUVScale, "UV scaling for the diffuse texture.")
        .def_readwrite("diffuseTextureScrollSpeed", &CGEngine::SurfaceParameters::diffuseTextureScrollSpeed, "Scrolling speed for the diffuse texture.")
        .def_readwrite("diffuseTextureOffset", &CGEngine::SurfaceParameters::diffuseTextureOffset, "Offset for the diffuse texture.")
        .def_readwrite("diffuseColor", &CGEngine::SurfaceParameters::diffuseColor, "Color of the diffuse reflection.")
        .def_readwrite("specularTexturePath", &CGEngine::SurfaceParameters::specularTexturePath, "Path to the specular texture.")
        .def_readwrite("specularTextureUVScale", &CGEngine::SurfaceParameters::specularTextureUVScale, "UV scaling for the specular texture.")
        .def_readwrite("specularTextureScrollSpeed", &CGEngine::SurfaceParameters::specularTextureScrollSpeed, "Scrolling speed for the specular texture.")
        .def_readwrite("specularTextureOffset", &CGEngine::SurfaceParameters::specularTextureOffset, "Offset for the specular texture.")
        .def_readwrite("specularColor", &CGEngine::SurfaceParameters::specularColor, "Color of the specular reflection.")
        .def_readwrite("smoothnessFactor", &CGEngine::SurfaceParameters::smoothnessFactor, "Smoothness factor for specular highlights.")
        .def_readwrite("opacityTexturePath", &CGEngine::SurfaceParameters::opacityTexturePath, "Path to the opacity texture.")
        .def_readwrite("opacityTextureUVScale", &CGEngine::SurfaceParameters::opacityTextureUVScale, "UV scaling for the opacity texture.")
        .def_readwrite("opacityTextureScrollSpeed", &CGEngine::SurfaceParameters::opacityTextureScrollSpeed, "Scrolling speed for the opacity texture.")
        .def_readwrite("opacityTextureOffset", &CGEngine::SurfaceParameters::opacityTextureOffset, "Offset for the opacity texture.")
        .def_readwrite("opacity", &CGEngine::SurfaceParameters::opacity, "Opacity of the material.")
        .def_readwrite("opacityMasked", &CGEngine::SurfaceParameters::opacityMasked, "Whether opacity masking is enabled.")
        .def_readwrite("alphaCutoff", &CGEngine::SurfaceParameters::alphaCutoff, "Alpha cutoff value for opacity masking.")
        .def_readwrite("useGammaCorrection", &CGEngine::SurfaceParameters::useGammaCorrection, "Whether gamma correction is applied.")
        .def_readwrite("gamma", &CGEngine::SurfaceParameters::gamma, "Gamma value for gamma correction.")
        .def_readwrite("useLighting", &CGEngine::SurfaceParameters::useLighting, "Whether lighting is enabled for the material.")
        .def_readwrite("useDiffuseTexture", &CGEngine::SurfaceParameters::useDiffuseTexture, "Whether diffuse texture is used.")
        .def_readwrite("useSpecularTexture", &CGEngine::SurfaceParameters::useSpecularTexture, "Whether specular texture is used.")
        .def_readwrite("useOpacityTexture", &CGEngine::SurfaceParameters::useOpacityTexture, "Whether opacity texture is used.");

        // Bind Material, inheriting IResource potentially if useful in Python?
    py::class_<CGEngine::Material, CGEngine::IResource, std::unique_ptr<CGEngine::Material, py::nodelete>>(m, "Material")
        .def_property_readonly("id", &CGEngine::Material::getId, "Get the unique ID of the material resource.")
        .def("get_program", &CGEngine::Material::getProgram, py::return_value_policy::reference, "Get the shader Program used by this material.")
        // Add .def("get_parameter", ...) etc. later if needed
        ;
}