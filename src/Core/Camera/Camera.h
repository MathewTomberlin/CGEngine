#pragma once

/// GLM is needed to provide 3D math properties, particularly matrices for 3D transformations.
#define GLM_ENABLE_EXPERIMENTAL
#include "glm.hpp"
#include "gtx/transform.hpp"
#include "SFML/Graphics.hpp"
using namespace sf;

namespace CGEngine {
	class Camera {
	public:
		Camera(Vector3f position, Vector3f target = {0,0,-1}, float fov = 93.f, float aspect = 1.f, float near = 1.f, float far = 500.f);
		Camera(float fov, float aspect, float near = 1.f, float far = 500.f);
		Camera(float aspect = 1.f);
		void setPosition(Vector3f pos, bool affect2DView = true);
		void setRotation(Vector3f rotation, bool affect2DView = true);
		void move(Vector3f delta, bool affect2DView = true);
		void rotate(Vector3f euler, bool affect2DView = true);
		glm::mat4 getProjection() {
			glm::mat4 proj = projection;// *orientation();
			proj = glm::translate(proj, glm::vec3(position.x, position.y, position.z));
			return proj;
		}
		glm::mat4 getTransform() {
			return transform;
		}
		glm::mat4 Camera::orientation() const;
	private:
		glm::vec3 position = { 0,0,0 };
		glm::vec3 rotation = { 0,0,-1 };
		float fov = 50.f;
		float aspect = 1.f;
		float nearPlane = 0.1f;
		float farPlane = 500.0f;
		glm::mat4 transform;
		glm::mat4 projection;
	};
}