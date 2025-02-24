#pragma once

#include "SFML/Graphics.hpp"
using namespace sf;
using namespace std;

namespace CGEngine {
	struct MaterialParameters {
		MaterialParameters(string texturePath = "", Color surfaceColor = Color::White, float shininess = 32.f, float opacity = 1.0f, Color specularColor = Color::White, bool gammaCorrected = false) :  texturePath(texturePath), surfaceColor(surfaceColor), shininess(shininess), opacity(opacity), specularColor(specularColor), gammaCorrected(gammaCorrected) {};
		
		string texturePath;
		float shininess = 1.0f;
		float opacity = 0.0f;
		Color specularColor = Color::White;
		Color surfaceColor = Color::White;
		bool gammaCorrected = false;
	};
	class Material {
	public:
		Material(MaterialParameters params = MaterialParameters());


		string texturePath;
		Texture* texture;
		float shininess = 1.0f;
		float opacity = 0.0f;
		Color specularColor = Color::White;
		Color surfaceColor = Color::White;
		bool gammaCorrected = false;
		id_t materialId;
	};
}