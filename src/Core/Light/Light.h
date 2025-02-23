#pragma once

#include "glm.hpp" 
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
		Light(Vector3f position = Vector3f(), Vector3f intensities = {1.0f,1.0f,1.0f});

		glm::vec3 position;
		glm::vec3 intensities;
	};
}