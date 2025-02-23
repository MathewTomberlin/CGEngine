#include "Camera.h"
#include "../Engine/Engine.h"
namespace CGEngine {
	static const float MaxVerticalCameraAngle = 85.0f; //must be less than 90 to avoid gimbal lock

	Camera::Camera(Vector3f position, float angleX, float angleY, float angleZ, float fov, float aspect, float nearPlane, float farPlane) : position(glm::vec3(position.x, position.y, position.z)), angleX(angleX), angleY(angleY), angleZ(angleZ), fov(fov), aspect(aspect), nearPlane(nearPlane), farPlane(farPlane) { };
	Camera::Camera(float fov, float aspect, float nearPlane, float farPlane) : Camera({ 0,0,1 }, 0.0f, 0.0f, 0.0f, fov, aspect, nearPlane, farPlane) { }
	Camera::Camera(float aspect) : Camera({0,0,1}, 0.0f, 0.0f, 0.0f, 93.f, aspect, 0.01f, 100.f) { }

	Vector3f Camera::getPosition() {
		return Vector3f({ position.x,position.y,position.z });
	}

	void Camera::setPosition(Vector3f pos, bool affect2DView) {
		this->position = { pos.x,pos.y,pos.z };

		if (affect2DView) {
			screen->setViewPosition({ pos.x,pos.y });
		}
	}

	void Camera::move(Vector3f delta, bool affect2DView) {
		position += glm::vec3{delta.x, delta.y, delta.z};

		if (affect2DView) {
			screen->moveView({ delta.x,delta.y });
		}
	}

	glm::mat4 Camera::getOrientation() const {
		glm::mat4 orientation;
		orientation = glm::rotate(glm::radians(angleZ), glm::vec3(0, 0, 1));
		orientation *= glm::rotate(glm::radians(angleY), glm::vec3(0, 1, 0));
		orientation *= glm::rotate(glm::radians(angleX), glm::vec3(1, 0, 0));
		return orientation;
	}

	void Camera::rotate(Vector3f euler, bool affect2DView) {
		angleX += euler.x;
		angleY += euler.y;
		angleZ += euler.z;
		normalizeAngles();
	}

	void Camera::lookAt(Vector3f position) {
		glm::vec3 pos(position.x, position.y, position.z);
		assert(pos != this->position);
		glm::vec3 direction = glm::normalize(pos - this->position);
		angleY = glm::radians(asinf(-direction.y));
		angleX = -glm::radians(atan2f(-direction.x, -direction.z));
		normalizeAngles();
	}

	glm::vec3 Camera::getForward() {
		glm::vec4 fwd = glm::inverse(getOrientation()) * glm::vec4(0, 0, -1, 1);
		return glm::vec3(fwd);
	}
	glm::vec3 Camera::getUp() {
		glm::vec4 up = glm::inverse(getOrientation()) * glm::vec4(0, 1, 0, 1);
		return glm::vec3(up);
	}
	glm::vec3 Camera::getRight() {
		glm::vec4 rgt = glm::inverse(getOrientation()) * glm::vec4(1, 0, 0, 1);
		return glm::vec3(rgt);
	}

	glm::mat4 Camera::getMatrix() const {
		return getProjection() * getTransform();
	}
	glm::mat4 Camera::getProjection() const {
		return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
	}
	glm::mat4 Camera::getTransform() const {
		return getOrientation() * glm::translate(-position);
	}

	void Camera::normalizeAngles() {
		angleY = fmodf(angleY, 360.0f);
		//fmodf can return negative values, but this will make them all positive
		if (angleY < 0.0f)
			angleY += 360.0f;

		angleZ = fmodf(angleZ, 360.0f);
		if (angleZ < 0.0f)
			angleZ += 360.0f;

		if (angleX > MaxVerticalCameraAngle) {
			angleX = MaxVerticalCameraAngle;
		} else if (angleX < -MaxVerticalCameraAngle) {
			angleX = -MaxVerticalCameraAngle;
		}
	}
}