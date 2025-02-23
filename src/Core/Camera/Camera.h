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
		Camera(Vector3f position, float angleX = 0.0f, float angleY = 0.0f, float angleZ = 0.0f, float fov = 93.f, float aspect = 1.f, float near = 1.f, float far = 500.f);
		Camera(float fov, float aspect, float near = 0.01f, float far = 100.f);
		Camera(float aspect = 1.f);
		Vector3f getPosition();
		void setPosition(Vector3f pos, bool affect2DView = true);
		void move(Vector3f delta, bool affect2DView = true);
		glm::mat4 getOrientation() const;
		void rotate(Vector3f euler, bool affect2DView = true);
		void lookAt(Vector3f position);
		glm::vec3 getForward();
		glm::vec3 getUp();
		glm::vec3 getRight();
		glm::mat4 getMatrix() const;
		glm::mat4 getProjection() const;
		glm::mat4 getTransform() const;
	private:
		glm::vec3 position = { 0,0,0 };
		float angleX;
		float angleY;
		float angleZ;
		float fov = 50.f;
		float aspect = 1.f;
		float nearPlane = 0.1f;
		float farPlane = 500.0f;
		void normalizeAngles();
	};
}