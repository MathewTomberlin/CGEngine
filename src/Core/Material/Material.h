#pragma once

#include "SFML/Graphics.hpp"
using namespace sf;
using namespace std;

namespace CGEngine {
	struct MaterialParameters {
		MaterialParameters(string diffuseTexturePath = "", Vector2f diffuseTextureUVScale = {1,1}, Color diffuseColor = Color::White, float shininess = 32.f, float opacity = 1.0f, Color specularColor = Color::White, bool gammaCorrected = false) : diffuseTexturePath(diffuseTexturePath), diffuseTextureUVScale(diffuseTextureUVScale), diffuseColor(diffuseColor), shininess(shininess), opacity(opacity), specularColor(specularColor), gammaCorrected(gammaCorrected) {};
		
		string diffuseTexturePath;
		Vector2f diffuseTextureUVScale = { 1,1 };
		float shininess = 1.0f;
		float opacity = 0.0f;
		Color specularColor = Color::White;
		Color diffuseColor = Color::White;
		bool gammaCorrected = false;
	};
	class Material {
	public:
		Material(MaterialParameters params = MaterialParameters());


		string diffuseTexturePath;
		Texture* diffuseTexture;
		Vector2f diffuseTextureUVScale = { 1,1 };
		float shininess = 1.0f;
		float opacity = 0.0f;
		Color specularColor = Color::White;
		Color diffuseColor = Color::White;
		bool gammaCorrected = false;
		id_t materialId;
	};
}