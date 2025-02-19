#pragma once

#include <SFML/OpenGL.hpp>
#include "../Engine/Engine.h"

namespace CGEngine {
	class Mesh : public Transformable{
	public:
		Mesh(vector<GLfloat> vertices, Texture* texture = nullptr, float depth = 0, bool screenSpaceRendering = false, bool textureCoordinatesEnabled = true);

		void render(Transform parentTransform);
		vector<GLfloat> vertices;
		Texture* meshTexture;
		float depth = 0;
		float height = 1;
	private:
		bool textureCoordinatesEnabled = true;
		bool screenSpaceRendering = false;
	};
}