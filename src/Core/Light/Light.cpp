#include "Light.h"
#include "../Engine/Engine.h"
namespace CGEngine {
	Light::Light(Vector3f position, bool directional, LightParameters parameters) : position(glm::vec4(position.x, position.y, position.z, (directional?0.0f:1.0f))), parameters(parameters) { 
		
	}

	bool Light::isValid() const {
		return true;
	}
}