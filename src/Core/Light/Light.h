#pragma once

#include "glm.hpp" 
#include "SFML/Graphics.hpp"
#include "../Types/Types.h"

namespace CGEngine {
	struct LightParameters {
		LightParameters(float brightness = 5.0f, Vector3f colorIntensities = {1,1,1}, float attenuation = 0.005f, float ambiance = 0.001f, float coneAngle = 180.0f, Vector3f lightDirection = Vector3f(0, 0, -1)) : brightness(brightness), colorIntensities(colorIntensities), attenuation(attenuation), ambiance(ambiance), coneAngle(coneAngle), lightDirection(lightDirection) {};
		float brightness;
		Vector3f colorIntensities;
		float attenuation;
		float ambiance;
		float coneAngle;
		Vector3f lightDirection;
	};

	class Light : public IResource {
	public:
		Light(Vector3f position = Vector3f(), bool directional = false, LightParameters parameters = LightParameters()) ;

		id_t lightId;
		glm::vec4 position;
		LightParameters parameters;

		bool isValid() const;
	};
}