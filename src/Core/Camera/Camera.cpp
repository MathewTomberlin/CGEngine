#include "Camera.h"
#include "../Engine/Engine.h"
namespace CGEngine {
	Camera::Camera(Vector3f position, Vector3f rotation, float fov, float aspect, float near, float far) :position(position), eulerRotation(rotation), fov(fov), aspect(aspect), nearPlane(near), farPlane(far) {
		projection = glm::perspective(fov, aspect, nearPlane, farPlane);
	};
	Camera::Camera(float fov, float aspect, float near, float far) : position(Vector3f()), eulerRotation(Vector3f()), fov(fov), aspect(aspect), nearPlane(near), farPlane(far) {
		projection = glm::perspective(fov, aspect, nearPlane, farPlane);
	}
	Camera::Camera(float aspect) : position(Vector3f()), eulerRotation(Vector3f()), fov(93.f), aspect(aspect), nearPlane(1.f), farPlane(500.f) {
		projection = glm::perspective(fov, aspect, nearPlane, farPlane);
	}

	void Camera::setPosition(Vector3f pos, bool affect2DView) {
		position = { -pos.x,pos.y,-pos.z };
		if (affect2DView) {
			screen->setViewPosition({ pos.x,pos.y });
		}
	}

	void Camera::setRotation(Vector3f rotation, bool affect2DView) {
		eulerRotation = rotation;
		if (affect2DView) {
			screen->setViewEulerRotation(rotation.z);
		}
	}

	void Camera::move(Vector3f delta, bool affect2DView) {
		position = position + Vector3f({-delta.x, delta.y, -delta.z});
		if (affect2DView) {
			screen->moveView({ delta.x,delta.y });
		}
	}

	void Camera::rotate(Vector3f euler, bool affect2DView) {
		eulerRotation = eulerRotation + euler;
		if (affect2DView) {
			screen->rotateView(degrees(euler.z));
		}
	}

	glm::mat4 Camera::orientation() const {
		glm::mat4 orientation;
		orientation = glm::rotate(orientation, eulerRotation.x, glm::vec3(1, 0, 0));
		orientation = glm::rotate(orientation, eulerRotation.y, glm::vec3(0, 1, 0));
		orientation = glm::rotate(orientation, eulerRotation.z, glm::vec3(0, 0, 1));
		return orientation;
	}
}