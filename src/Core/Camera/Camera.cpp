#include "Camera.h"
#include "../Engine/Engine.h"
namespace CGEngine {
	Camera::Camera(Vector3f position, Vector3f rotation, float fov, float aspect, float nearPlane, float farPlane) : position(glm::vec3(position.x, position.y, position.z)), rotation(glm::vec3(rotation.x, rotation.y, rotation.z)), fov(fov), aspect(aspect), nearPlane(nearPlane), farPlane(farPlane) {
		transform = glm::lookAt(this->position, this->position + this->rotation, glm::vec3(0, 1, 0));
		projection = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
	};
	Camera::Camera(float fov, float aspect, float nearPlane, float farPlane) : Camera({ 0,0,0 }, { 0,0,-1 },fov,aspect, nearPlane,farPlane) { }
	Camera::Camera(float aspect) : Camera({0,0,0}, {0,0,-1}, 50.f, aspect, 0.1f, 500.f) { }

	void Camera::setPosition(Vector3f pos, bool affect2DView) {
		this->position = { -pos.x,pos.y,-pos.z };
		if (affect2DView) {
			screen->setViewPosition({ pos.x,pos.y });
		}
	}

	void Camera::setRotation(Vector3f rotation, bool affect2DView) {
		this->rotation = { rotation.x,rotation.y,rotation.z };
		if (affect2DView) {
			screen->setViewEulerRotation(rotation.z);
		}
	}

	void Camera::move(Vector3f delta, bool affect2DView) {
		position = position + glm::vec3{-delta.x, delta.y, -delta.z};
		if (affect2DView) {
			screen->moveView({ delta.x,delta.y });
		}
	}

	void Camera::rotate(Vector3f euler, bool affect2DView) {
		rotation = rotation + glm::vec3{ euler.x,euler.y,euler.z };
		if (affect2DView) {
			screen->rotateView(degrees(euler.z));
		}
	}

	glm::mat4 Camera::orientation() const {
		glm::mat4 orientation;
		orientation = glm::rotate(orientation, rotation.x, glm::vec3(1, 0, 0));
		orientation = glm::rotate(orientation, rotation.y, glm::vec3(0, 1, 0));
		orientation = glm::rotate(orientation, rotation.z, glm::vec3(0, 0, 1));
		return orientation;
	}
}