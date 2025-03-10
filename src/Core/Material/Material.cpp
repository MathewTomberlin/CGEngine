#include "../Engine/Engine.h"
#include "Material.h"

namespace CGEngine {
	Material::Material(ShaderProgramPath shaderPath) {
		//TODO: Should materials using the same two shaders also use the same shader ref?
		if (renderer.setGLWindowState(true)) {
			shaderProgram = new Program(shaderPath);
			renderer.setGLWindowState(false);
		}

		materialId = world->addMaterial(this);
	}
	Material::Material(SurfaceParameters params, ShaderProgramPath shaderPath) : Material(shaderPath) {
		setParameter("diffuseTexturePath", params.diffuseTexturePath, ParamType::String);
		setParameter("diffuseTextureUVScale", params.diffuseTextureUVScale, ParamType::V2);
		setParameter("diffuseTextureScrollSpeed", params.diffuseTextureScrollSpeed, ParamType::V2);
		setParameter("diffuseTextureOffset", params.diffuseTextureOffset, ParamType::V2);
		setParameter("diffuseTexture", textures->load(params.diffuseTexturePath), ParamType::Texture2D);
		setParameter("diffuseColor", params.diffuseColor, ParamType::RGBA);
		setParameter("specularTexturePath", params.specularTexturePath, ParamType::String);
		setParameter("specularTextureUVScale", params.specularTextureUVScale, ParamType::V2);
		setParameter("specularTextureScrollSpeed", params.specularTextureScrollSpeed, ParamType::V2);
		setParameter("specularTextureOffset", params.specularTextureOffset, ParamType::V2);
		setParameter("specularTexture", textures->load(params.specularTexturePath), ParamType::Texture2D);
		setParameter("specularColor", params.specularColor, ParamType::RGBA);
		setParameter("smoothnessFactor", params.smoothnessFactor, ParamType::Float);
		setParameter("opacityTextureUVScale", params.opacityTextureUVScale, ParamType::V2);
		setParameter("opacityTextureScrollSpeed", params.opacityTextureScrollSpeed, ParamType::V2);
		setParameter("opacityTextureOffset", params.opacityTextureOffset, ParamType::V2);
		setParameter("opacityTexture", textures->load(params.opacityTexturePath), ParamType::Texture2D);
		setParameter("opacity", params.opacity, ParamType::Float);
		setParameter("opacityMasked", params.opacityMasked, ParamType::Bool);
		setParameter("alphaCutoff", params.alphaCutoff, ParamType::Float);
		setParameter("gamma", params.gamma, ParamType::Float);
		setParameter("useGammaCorrection", params.useGammaCorrection, ParamType::Bool);
		setParameter("useDiffuseTexture", params.useDiffuseTexture, ParamType::Bool);
		setParameter("useSpecularTexture", params.useSpecularTexture, ParamType::Bool);
		setParameter("useOpacityTexture", params.useOpacityTexture, ParamType::Bool);
		setParameter("useLighting", params.useLighting, ParamType::Bool);
	};

	Material::Material(map<string, ParamData> materialParameters, ShaderProgramPath shaderPath) : Material(shaderPath) {
		this->materialParameters = materialParameters;
	}

	void Material::setParameter(string paramName, any paramValue, ParamType paramType) {
		materialParameters[paramName] = ParamData(paramValue, paramType);
	}

	optional<ParamData> Material::getParameter(string paramName) {
		auto iterator = materialParameters.find(paramName);
		if (iterator != materialParameters.end()) {
			return (*iterator).second;
		}
		cout << "Material parameter " << paramName << " with not found";
		return nullopt;
	}

	void Material::removeParameter(string paramName) {
		auto iterator = materialParameters.find(paramName);
		if (iterator != materialParameters.end()) {
			materialParameters.erase(paramName);
		}
	}

	Program* Material::getProgram() {
		return shaderProgram;
	}
}