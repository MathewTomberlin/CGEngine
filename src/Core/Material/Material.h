#pragma once

#include <any>
#include <optional>
#include "SFML/Graphics.hpp"
using namespace sf;
using namespace std;

namespace CGEngine {
	struct SurfaceDomain {
		SurfaceDomain(float domainIntensity = 1.0f, Color domainColor = Color::White, string texturePath = "", Vector2f textureScale = { 1,1 }) : texturePath(texturePath), textureScale(textureScale), domainColor(domainColor), domainIntensity(domainIntensity) {};
		SurfaceDomain(string texturePath, float domainIntensity = 1.0f, Color domainColor = Color::White, Vector2f textureScale = {1,1}) : SurfaceDomain(domainIntensity, domainColor, texturePath, textureScale) {};
		SurfaceDomain(string texturePath, Vector2f textureScale, Color domainColor = Color::White, float domainIntensity = 1.0f) : SurfaceDomain(domainIntensity, domainColor, texturePath, textureScale) {};
		SurfaceDomain(string texturePath, Color domainColor, float domainIntensity = 1.0f, Vector2f textureScale = { 1,1 }) : SurfaceDomain(domainIntensity, domainColor, texturePath, textureScale) {};

		string texturePath;
		Vector2f textureScale;
		Color domainColor;
		float domainIntensity;
	};

	struct SurfaceParameters {
		SurfaceParameters(string diffuseTexturePath = "", Vector2f diffuseTextureUVScale = { 1,1 }, Color diffuseColor = Color::White, string specularTexturePath = "", Vector2f specularTextureUVScale = { 1,1 }, Color specularColor = Color::White, float smoothnessFactor = 32.f, string opacityTexturePath = "", Vector2f opacityTextureUVScale = { 1,1 }, float opacity = 1.0f, float alphaCutoff = 0.01f, bool useGammaCorrection = false, float gamma = 2.2f, bool useLighting = true) :
			diffuseTexturePath(diffuseTexturePath),
			diffuseTextureUVScale(diffuseTextureUVScale),
			diffuseColor(diffuseColor),
			specularTexturePath(specularTexturePath),
			specularTextureUVScale(specularTextureUVScale),
			smoothnessFactor(smoothnessFactor),
			opacityTexturePath(opacityTexturePath),
			opacityTextureUVScale(opacityTextureUVScale),
			opacity(opacity),
			specularColor(specularColor),
			gamma(gamma),
			useGammaCorrection(useGammaCorrection),
			useDiffuseTexture(diffuseTexturePath != ""),
			useSpecularTexture(specularTexturePath != ""),
			useOpacityTexture(opacityTexturePath != ""),
			alphaCutoff(alphaCutoff),
			useLighting(useLighting)
		{ };

		SurfaceParameters(SurfaceDomain diffuseDomain = SurfaceDomain(), SurfaceDomain specularDomain = SurfaceDomain(32.0f), SurfaceDomain opacityDomain = SurfaceDomain(), float alphaCutoff = 0.01f, bool useGammaCorrection = false, float gamma = 2.2f, bool useLighting = true) :
			SurfaceParameters(diffuseDomain.texturePath, diffuseDomain.textureScale, diffuseDomain.domainColor, specularDomain.texturePath, specularDomain.textureScale, specularDomain.domainColor, specularDomain.domainIntensity, opacityDomain.texturePath, opacityDomain.textureScale, opacityDomain.domainIntensity, alphaCutoff, useGammaCorrection, gamma, useLighting) {};
		
		string diffuseTexturePath = "";
		Vector2f diffuseTextureUVScale = { 1,1 };
		Color diffuseColor = Color::White;
		float smoothnessFactor = 1.0f;
		string specularTexturePath = "";
		Vector2f specularTextureUVScale = { 1,1 };
		Color specularColor = Color::White;
		string opacityTexturePath = "";
		Vector2f opacityTextureUVScale;
		float opacity = 0.0f;
		float alphaCutoff = 0.01f;
		float gamma = 2.2f;
		bool useGammaCorrection = false;
		bool useDiffuseTexture = true;
		bool useSpecularTexture = false;
		bool useOpacityTexture = false;
		bool useLighting = true;
	};
	class Material {
	public:
		Material(ShaderProgramPath shaderPath = ShaderProgramPath());
		Material(SurfaceParameters params, ShaderProgramPath shaderPath = ShaderProgramPath());
		Material(map<string, ParamData> materialParameters, ShaderProgramPath shaderPath = ShaderProgramPath());

		template<typename T>
		optional<T> getParameter(string paramName) {
			auto iterator = materialParameters.find(paramName);
			if (iterator != materialParameters.end()) {
				return any_cast<T>((*iterator).second.data);
			}
			cout << "Material parameter " << paramName << " not found";
			return nullopt;
		}

		template<typename T>
		T* getParameterPtr(string paramName) {
			auto iterator = materialParameters.find(paramName);
			if (iterator != materialParameters.end()) {
				return any_cast<T*>((*iterator).second.data);
			}
			cout << "Material parameter " << paramName << " not found";
			return nullptr;
		}

		optional<ParamData> getParameter(string paramName);
		void setParameter(string paramName, any paramValue, ParamType paramType);
		void removeParameter(string paramName);
		Program* getProgram();
		id_t materialId;
	private:
		friend class Renderer;
		map<string, ParamData> materialParameters;
		Program* shaderProgram = nullptr;
	};
}