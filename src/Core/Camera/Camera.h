#pragma once

#include "SFML/Graphics.hpp"
using namespace sf;

namespace CGEngine {
	class Camera {
	public:
		Camera(Vector3f position, Vector3f rotation) :position(position), eulerRotation(rotation) {};

		void setPosition(Vector3f pos, bool affect2DView = true);
		void setRotation(Vector3f rotation, bool affect2DView = true);
		void move(Vector3f delta, bool affect2DView = true);
		void rotate(Vector3f euler, bool affect2DView = true);
		Vector3f position;
		Vector3f eulerRotation;
	};
}