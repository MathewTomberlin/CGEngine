#include "../Engine/Engine.h"
#include "Material.h"

namespace CGEngine {
	Material::Material(ShaderProgramPath shaderPath) {
		//TODO: Should materials using the same two shaders also use the same shader ref?
		if (renderer.setGLWindowState(true)) {
			shaderProgram = new Program(shaderPath);
		}
	}

	Material::Material() { }

	Material::Material(SurfaceParameters params) :Material() {
		auto diffuseAsset = assets.load<TextureResource>(params.diffuseTexturePath);
		optional<id_t> diffuseTextureId = nullopt;
		if (diffuseAsset.has_value()) {
			diffuseTextureId = diffuseAsset.value().first;
		}
		auto specularAsset = assets.load<TextureResource>(params.specularTexturePath);
		optional<id_t> specularTextureId = nullopt;
		if(specularAsset.has_value()){
			specularTextureId = specularAsset.value().first;
		}
		auto opacityAsset = assets.load<TextureResource>(params.opacityTexturePath);
		optional<id_t> opacityTextureId = nullopt;
		if (opacityAsset.has_value()) {
			opacityTextureId = opacityAsset.value().first;
		}

		if (diffuseTextureId.has_value()) {
			TextureResource* diffuseTexture = assets.get<TextureResource>(diffuseTextureId.value());
			if (diffuseTexture) {
				setParameter("diffuseTexture", diffuseTexture->getTexture(), ParamType::Texture2D);
			}
		}
		if (specularTextureId.has_value()) {
			TextureResource* specularTexture = assets.get<TextureResource>(specularTextureId.value());
			if (specularTexture) {
				setParameter("specularTexture", specularTexture->getTexture(), ParamType::Texture2D);
			}
		}
		if (opacityTextureId.has_value()) {
			TextureResource* opacityTexture = assets.get<TextureResource>(opacityTextureId.value());
			if (opacityTexture) {
				setParameter("opacityTexture", opacityTexture->getTexture(), ParamType::Texture2D);
			}
		}

		setParameter("diffuseTexturePath", params.diffuseTexturePath, ParamType::String);
		setParameter("diffuseTextureUVScale", params.diffuseTextureUVScale, ParamType::V2);
		setParameter("diffuseTextureScrollSpeed", params.diffuseTextureScrollSpeed, ParamType::V2);
		setParameter("diffuseTextureOffset", params.diffuseTextureOffset, ParamType::V2);
		setParameter("diffuseColor", params.diffuseColor, ParamType::RGBA);
		setParameter("specularTexturePath", params.specularTexturePath, ParamType::String);
		setParameter("specularTextureUVScale", params.specularTextureUVScale, ParamType::V2);
		setParameter("specularTextureScrollSpeed", params.specularTextureScrollSpeed, ParamType::V2);
		setParameter("specularTextureOffset", params.specularTextureOffset, ParamType::V2);
		setParameter("specularColor", params.specularColor, ParamType::RGBA);
		setParameter("smoothnessFactor", params.smoothnessFactor, ParamType::Float);
		setParameter("opacityTextureUVScale", params.opacityTextureUVScale, ParamType::V2);
		setParameter("opacityTextureScrollSpeed", params.opacityTextureScrollSpeed, ParamType::V2);
		setParameter("opacityTextureOffset", params.opacityTextureOffset, ParamType::V2);
		setParameter("opacity", params.opacity, ParamType::Float);
		setParameter("opacityMasked", params.opacityMasked, ParamType::Bool);
		setParameter("alphaCutoff", params.alphaCutoff, ParamType::Float);
		setParameter("gamma", params.gamma, ParamType::Float);
		setParameter("useGammaCorrection", params.useGammaCorrection, ParamType::Bool);
		setParameter("useDiffuseTexture", params.useDiffuseTexture, ParamType::Bool);
		setParameter("useSpecularTexture", params.useSpecularTexture, ParamType::Bool);
		setParameter("useOpacityTexture", params.useOpacityTexture, ParamType::Bool);
		setParameter("useLighting", params.useLighting, ParamType::Bool);
	}

	Material::Material(map<string, ParamData> materialParameters):Material() {
		this->materialParameters = materialParameters;
	}

	Material::Material(SurfaceParameters params, ShaderProgramPath shaderPath) : Material(shaderPath) {
		auto diffuseAsset = assets.load<TextureResource>(params.diffuseTexturePath);
		optional<id_t> diffuseTextureId = nullopt;
		if (diffuseAsset.has_value()) {
			diffuseTextureId = diffuseAsset.value().first;
		}
		auto specularAsset = assets.load<TextureResource>(params.specularTexturePath);
		optional<id_t> specularTextureId = nullopt;
		if (specularAsset.has_value()) {
			specularTextureId = specularAsset.value().first;
		}
		auto opacityAsset = assets.load<TextureResource>(params.opacityTexturePath);
		optional<id_t> opacityTextureId = nullopt;
		if (opacityAsset.has_value()) {
			opacityTextureId = opacityAsset.value().first;
		}
		if (diffuseTextureId.has_value()) {
			TextureResource* diffuseTexture = assets.get<TextureResource>(diffuseTextureId.value());
			if (diffuseTexture) {
				setParameter("diffuseTexture", diffuseTexture->getTexture(), ParamType::Texture2D);
			}
		}
		if (specularTextureId.has_value()) {
			TextureResource* specularTexture = assets.get<TextureResource>(specularTextureId.value());
			if (specularTexture) {
				setParameter("specularTexture", specularTexture->getTexture(), ParamType::Texture2D);
			}
		}
		if (opacityTextureId.has_value()) {
			TextureResource* opacityTexture = assets.get<TextureResource>(opacityTextureId.value());
			if (opacityTexture) {
				setParameter("opacityTexture", opacityTexture->getTexture(), ParamType::Texture2D);
			}
		}

		setParameter("diffuseTexturePath", params.diffuseTexturePath, ParamType::String);
		setParameter("diffuseTextureUVScale", params.diffuseTextureUVScale, ParamType::V2);
		setParameter("diffuseTextureScrollSpeed", params.diffuseTextureScrollSpeed, ParamType::V2);
		setParameter("diffuseTextureOffset", params.diffuseTextureOffset, ParamType::V2);
		setParameter("diffuseColor", params.diffuseColor, ParamType::RGBA);
		setParameter("specularTexturePath", params.specularTexturePath, ParamType::String);
		setParameter("specularTextureUVScale", params.specularTextureUVScale, ParamType::V2);
		setParameter("specularTextureScrollSpeed", params.specularTextureScrollSpeed, ParamType::V2);
		setParameter("specularTextureOffset", params.specularTextureOffset, ParamType::V2);
		setParameter("specularColor", params.specularColor, ParamType::RGBA);
		setParameter("smoothnessFactor", params.smoothnessFactor, ParamType::Float);
		setParameter("opacityTextureUVScale", params.opacityTextureUVScale, ParamType::V2);
		setParameter("opacityTextureScrollSpeed", params.opacityTextureScrollSpeed, ParamType::V2);
		setParameter("opacityTextureOffset", params.opacityTextureOffset, ParamType::V2);
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

	Material::Material(SurfaceParameters params, Program* program) : shaderProgram(program) {
		auto diffuseAsset = assets.load<TextureResource>(params.diffuseTexturePath);
		optional<id_t> diffuseTextureId = nullopt;
		if (diffuseAsset.has_value()) {
			diffuseTextureId = diffuseAsset.value().first;
		}
		auto specularAsset = assets.load<TextureResource>(params.specularTexturePath);
		optional<id_t> specularTextureId = nullopt;
		if (specularAsset.has_value()) {
			specularTextureId = specularAsset.value().first;
		}
		auto opacityAsset = assets.load<TextureResource>(params.opacityTexturePath);
		optional<id_t> opacityTextureId = nullopt;
		if (opacityAsset.has_value()) {
			opacityTextureId = opacityAsset.value().first;
		}
		if (diffuseTextureId.has_value()) {
			TextureResource* diffuseTexture = assets.get<TextureResource>(diffuseTextureId.value());
			if (diffuseTexture) {
				setParameter("diffuseTexture", diffuseTexture->getTexture(), ParamType::Texture2D);
			}
		}
		if (specularTextureId.has_value()) {
			TextureResource* specularTexture = assets.get<TextureResource>(specularTextureId.value());
			if (specularTexture) {
				setParameter("specularTexture", specularTexture->getTexture(), ParamType::Texture2D);
			}
		}
		if (opacityTextureId.has_value()) {
			TextureResource* opacityTexture = assets.get<TextureResource>(opacityTextureId.value());
			if (opacityTexture) {
				setParameter("opacityTexture", opacityTexture->getTexture(), ParamType::Texture2D);
			}
		}
		setParameter("diffuseTexturePath", params.diffuseTexturePath, ParamType::String);
		setParameter("diffuseTextureUVScale", params.diffuseTextureUVScale, ParamType::V2);
		setParameter("diffuseTextureScrollSpeed", params.diffuseTextureScrollSpeed, ParamType::V2);
		setParameter("diffuseTextureOffset", params.diffuseTextureOffset, ParamType::V2);
		setParameter("diffuseColor", params.diffuseColor, ParamType::RGBA);
		setParameter("specularTexturePath", params.specularTexturePath, ParamType::String);
		setParameter("specularTextureUVScale", params.specularTextureUVScale, ParamType::V2);
		setParameter("specularTextureScrollSpeed", params.specularTextureScrollSpeed, ParamType::V2);
		setParameter("specularTextureOffset", params.specularTextureOffset, ParamType::V2);
		setParameter("specularColor", params.specularColor, ParamType::RGBA);
		setParameter("smoothnessFactor", params.smoothnessFactor, ParamType::Float);
		setParameter("opacityTextureUVScale", params.opacityTextureUVScale, ParamType::V2);
		setParameter("opacityTextureScrollSpeed", params.opacityTextureScrollSpeed, ParamType::V2);
		setParameter("opacityTextureOffset", params.opacityTextureOffset, ParamType::V2);
		setParameter("opacity", params.opacity, ParamType::Float);
		setParameter("opacityMasked", params.opacityMasked, ParamType::Bool);
		setParameter("alphaCutoff", params.alphaCutoff, ParamType::Float);
		setParameter("gamma", params.gamma, ParamType::Float);
		setParameter("useGammaCorrection", params.useGammaCorrection, ParamType::Bool);
		setParameter("useDiffuseTexture", params.useDiffuseTexture, ParamType::Bool);
		setParameter("useSpecularTexture", params.useSpecularTexture, ParamType::Bool);
		setParameter("useOpacityTexture", params.useOpacityTexture, ParamType::Bool);
		setParameter("useLighting", params.useLighting, ParamType::Bool);
	}
	Material::Material(map<string, ParamData> materialParameters, Program* program) : shaderProgram(program) {
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