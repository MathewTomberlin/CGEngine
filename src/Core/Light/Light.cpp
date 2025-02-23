#include "Light.h"

namespace CGEngine {
	Light::Light(Vector3f position, Vector3f intensities) : position(glm::vec3(position.x, position.y, position.z)), intensities(glm::vec3(intensities.x,intensities.y,intensities.z)) { }
}