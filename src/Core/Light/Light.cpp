#include "Light.h"
#include "../Engine/Engine.h"
#include "SFML/OpenGL.hpp"

namespace CGEngine {
	Light::Light(float brightness, Color diffuse, Color ambient, Color specular, Vector3f pos, Vector3f rotation, bool directional, SpotlightParameters spotlight) : brightness(brightness), directional(directional), spotlightParameters(spotlight) {
		diffuseColor = { ((float)diffuse.r/255)*brightness,((float)diffuse.g/255) * brightness,((float)diffuse.b/255) * brightness,(float)diffuse.a };
		ambientColor = { (float)ambient.r,(float)ambient.g,(float)ambient.b,(float)ambient.a };
		specularColor = { ((float)specular.r/255) * brightness,((float)specular.g/255) * brightness,((float)specular.b/255) *brightness,(float)specular.a };
		position = { (float)pos.x,(float)pos.y,(float)pos.z,((directional)?0.f:1.f)};
		eulerRotation = { (float)rotation.x,(float)rotation.y,(float)rotation.z };
		lightId = world->addLight(this);
	}

	void Light::init() {
		if (renderer.setGLWindowState(true)) {
			GLenum lightIndex = GL_LIGHT0 + lightId;
			glEnable(lightIndex);
			glLightfv(lightIndex, GL_POSITION, position.data());
			glLightfv(lightIndex, GL_DIFFUSE, diffuseColor.data());
			glLightfv(lightIndex, GL_AMBIENT, ambientColor.data());
			glLightfv(lightIndex, GL_SPECULAR, specularColor.data());
			glLightfv(lightIndex, GL_SPOT_DIRECTION, eulerRotation.data());
			glLightf(lightIndex, GL_SPOT_CUTOFF, spotlightParameters.spotCutoff);
			glLightf(lightIndex, GL_SPOT_EXPONENT, spotlightParameters.spotExponent);
			glLightf(lightIndex, GL_CONSTANT_ATTENUATION, spotlightParameters.spotConstantAttenuation);
			glLightf(lightIndex, GL_LINEAR_ATTENUATION, spotlightParameters.spotLinearAttenuation);
			glLightf(lightIndex, GL_QUADRATIC_ATTENUATION, spotlightParameters.spotQuadraticAttenuation);
		}
	}
}