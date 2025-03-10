#pragma once

#include <any>
#include <optional>
#include "SFML/Graphics.hpp"
using namespace sf;
using namespace std;

namespace CGEngine {
	struct SurfaceDomain {
		SurfaceDomain(float domainIntensity = 1.0f, Color domainColor = Color::White, string texturePath = "", Vector2f textureScale = { 1,1 }, Vector2f textureScrollSpeed = { 0,0 }, Vector2f textureOffset = { 0,0 }) : texturePath(texturePath), textureScale(textureScale), domainColor(domainColor), domainIntensity(domainIntensity), textureScrollSpeed(textureScrollSpeed), textureOffset(textureOffset){};
		SurfaceDomain(string texturePath, float domainIntensity = 1.0f, Color domainColor = Color::White, Vector2f textureScale = {1,1}, Vector2f textureScrollSpeed = { 0,0 }, Vector2f textureOffset = { 0,0 }) : SurfaceDomain(domainIntensity, domainColor, texturePath, textureScale, textureScrollSpeed, textureOffset) {};
		SurfaceDomain(string texturePath, Vector2f textureScale, Color domainColor = Color::White, float domainIntensity = 1.0f, Vector2f textureScrollSpeed = { 0,0 }, Vector2f textureOffset = { 0,0 }) : SurfaceDomain(domainIntensity, domainColor, texturePath, textureScale, textureScrollSpeed, textureOffset) {};
		SurfaceDomain(string texturePath, Color domainColor, float domainIntensity = 1.0f, Vector2f textureScale = { 1,1 }, Vector2f textureScrollSpeed = { 0,0 }, Vector2f textureOffset = { 0,0 }) : SurfaceDomain(domainIntensity, domainColor, texturePath, textureScale, textureScrollSpeed, textureOffset) {};

		string texturePath;
		Vector2f textureScale;
		Vector2f textureScrollSpeed;
		Vector2f textureOffset;
		Color domainColor;
		float domainIntensity;
	};

	struct SurfaceParameters {
		SurfaceParameters(string diffuseTexturePath = "", Vector2f diffuseTextureUVScale = { 1,1 }, Vector2f diffuseTextureScrollSpeed = { 0,0 }, Vector2f diffuseTextureOffset = { 0,0 }, Color diffuseColor = Color::White, string specularTexturePath = "", Vector2f specularTextureUVScale = { 1,1 }, Vector2f specularTextureScrollSpeed = { 0,0 }, Vector2f specularTextureOffset = { 0,0 }, Color specularColor = Color::White, float smoothnessFactor = 32.f, string opacityTexturePath = "", Vector2f opacityTextureUVScale = { 1,1 }, Vector2f opacityTextureOffset = { 0,0 }, Vector2f opacityTextureScrollSpeed = { 0,0 }, float opacity = 1.0f, bool opacityMasked = true, float alphaCutoff = 0.01f, bool useGammaCorrection = false, float gamma = 2.2f, bool useLighting = true) :
			diffuseTexturePath(diffuseTexturePath),
			diffuseTextureUVScale(diffuseTextureUVScale),
			diffuseTextureScrollSpeed(diffuseTextureScrollSpeed),
			diffuseTextureOffset(diffuseTextureOffset),
			diffuseColor(diffuseColor),
			specularTexturePath(specularTexturePath),
			specularTextureUVScale(specularTextureUVScale),
			specularTextureScrollSpeed(specularTextureScrollSpeed),
			specularTextureOffset(specularTextureOffset),
			smoothnessFactor(smoothnessFactor),
			opacityTexturePath(opacityTexturePath),
			opacityTextureUVScale(opacityTextureUVScale),
			opacityTextureScrollSpeed(opacityTextureScrollSpeed),
			opacityTextureOffset(opacityTextureOffset),
			opacity(opacity),
			specularColor(specularColor),
			gamma(gamma),
			useGammaCorrection(useGammaCorrection),
			useDiffuseTexture(diffuseTexturePath != ""),
			useSpecularTexture(specularTexturePath != ""),
			useOpacityTexture(opacityTexturePath != ""),
			opacityMasked(opacityMasked),
			alphaCutoff(alphaCutoff),
			useLighting(useLighting)
		{ };

		SurfaceParameters(SurfaceDomain diffuseDomain = SurfaceDomain(), SurfaceDomain specularDomain = SurfaceDomain(32.0f), SurfaceDomain opacityDomain = SurfaceDomain(), bool opacityMasked = true, float alphaCutoff = 0.01f, bool useGammaCorrection = false, float gamma = 2.2f, bool useLighting = true) :
			SurfaceParameters(diffuseDomain.texturePath, diffuseDomain.textureScale, diffuseDomain.textureScrollSpeed, diffuseDomain.textureOffset, diffuseDomain.domainColor, specularDomain.texturePath, specularDomain.textureScale, specularDomain.textureScrollSpeed, specularDomain.textureOffset, specularDomain.domainColor, specularDomain.domainIntensity, opacityDomain.texturePath, opacityDomain.textureScale, opacityDomain.textureScrollSpeed, opacityDomain.textureOffset, opacityDomain.domainIntensity, opacityMasked, alphaCutoff, useGammaCorrection, gamma, useLighting) {};
		
		string diffuseTexturePath = "";
		Vector2f diffuseTextureUVScale = { 1,1 };
		Vector2f diffuseTextureScrollSpeed = { 0,0 };
		Vector2f diffuseTextureOffset = { 0,0 };
		Color diffuseColor = Color::White;
		float smoothnessFactor = 1.0f;
		string specularTexturePath = "";
		Vector2f specularTextureUVScale = { 1,1 };
		Vector2f specularTextureScrollSpeed = { 0,0 };
		Vector2f specularTextureOffset = { 0,0 };
		Color specularColor = Color::White;
		string opacityTexturePath = "";
		Vector2f opacityTextureUVScale;
		Vector2f opacityTextureScrollSpeed = { 0,0 };
		Vector2f opacityTextureOffset = { 0,0 };
		float opacity = 0.0f;
		bool opacityMasked = true;
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