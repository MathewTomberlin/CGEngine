#include "Camera.h"
#include "../Engine/Engine.h"
namespace CGEngine {
	void Camera::setPosition(Vector3f pos, bool affect2DView) {
		position = pos;
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
		position = position + delta;
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
}