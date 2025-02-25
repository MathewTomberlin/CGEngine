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
		setParameter("diffuseTexture", textures->load(params.diffuseTexturePath), ParamType::Texture2D);
		setParameter("smoothnessFactor", params.smoothnessFactor, ParamType::Float);
		setParameter("opacity", params.opacity, ParamType::Float);
		setParameter("specularColor", params.specularColor, ParamType::RGBA);
		setParameter("diffuseColor", params.diffuseColor, ParamType::RGBA);
		setParameter("gammaCorrected", params.gammaCorrected, ParamType::Bool);
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