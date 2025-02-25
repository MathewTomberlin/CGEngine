#pragma once

#include <any>
#include <optional>
#include "SFML/Graphics.hpp"
using namespace sf;
using namespace std;

namespace CGEngine {
	struct SurfaceParameters {
		SurfaceParameters(string diffuseTexturePath = "", Vector2f diffuseTextureUVScale = {1,1}, Color diffuseColor = Color::White, float smoothnessFactor = 32.f, float opacity = 1.0f, Color specularColor = Color::White, bool gammaCorrected = false) : diffuseTexturePath(diffuseTexturePath), diffuseTextureUVScale(diffuseTextureUVScale), diffuseColor(diffuseColor), smoothnessFactor(smoothnessFactor), opacity(opacity), specularColor(specularColor), gammaCorrected(gammaCorrected) {
			params["diffuseTexturePath"] = diffuseTexturePath;
			params["diffuseTextureUVScale"] = diffuseTextureUVScale;
			params["smoothnessFactor"] = smoothnessFactor;
			params["opacity"] = opacity;
			params["specularColor"] = specularColor;
			params["diffuseColor"] = diffuseColor;
			params["gammaCorrected"] = gammaCorrected;
		};
		
		string diffuseTexturePath;
		Vector2f diffuseTextureUVScale = { 1,1 };
		float smoothnessFactor = 1.0f;
		float opacity = 0.0f;
		Color specularColor = Color::White;
		Color diffuseColor = Color::White;
		bool gammaCorrected = false;
		map<string, any> params;
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