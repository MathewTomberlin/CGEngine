#pragma once

#include "SFML/Graphics.hpp"
#include "../Types/Types.h"
using namespace sf;
using namespace std;

namespace CGEngine {
	struct SpotlightParameters {
		SpotlightParameters(float cutoff = 60.f, float exponent = 128.f, float constantAtten = 1.f, float linearAtten = 0.1f, float quadraticAtten = 0.1f) :spotCutoff(cutoff), spotExponent(exponent), spotConstantAttenuation(constantAtten), spotLinearAttenuation(linearAtten), spotQuadraticAttenuation(quadraticAtten) {};
		float spotCutoff = 60.f;
		float spotExponent = 128.f;
		float spotConstantAttenuation = 1.f;
		float spotLinearAttenuation = 0.1f;
		float spotQuadraticAttenuation = 0.1f;
	};

	class Light {
	public:
		Light(float brightness = 5.f, Color diffuse = Color::White, Color ambient = Color::Black, Color specular = Color::White, Vector3f pos = {0,0,5}, Vector3f rotation = {0,0,-1}, bool directional = false, SpotlightParameters spotlight = SpotlightParameters());

		void init();

		float brightness = 5.f;
		vector<float> diffuseColor = {1,1,1,1};
		vector<float>  ambientColor = { 0,0,0,1 };
		vector<float>  specularColor = { 1,1,1,1 };
		vector<float>  position = { 0,0,5 };
		vector<float>  eulerRotation = { 0,0,-1 };
		bool directional = false;
		SpotlightParameters spotlightParameters;
		id_t lightId;
	};
}