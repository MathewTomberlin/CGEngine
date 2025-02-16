#pragma once

#include <SFML/OpenGL.hpp>
#include "../Engine/Engine.h"

namespace CGEngine {
	class Mesh : public Transformable{
	public:
		Mesh(vector<GLfloat> vertices, Texture* texture = nullptr, bool textureCoordinatesEnabled = true, float depth = 0);

		void render(Transform parentTransform);
		Vector2f position;
		vector<GLfloat> vertices;
		Texture* meshTexture;
		float depth = 0;
		float height = 1;
	private:
		bool textureCoordinatesEnabled = true;
	};
}