#pragma once

#include <SFML/OpenGL.hpp>
#include "../Types/V3.h"
#include "../Engine/Engine.h"

namespace CGEngine {
	class Mesh : public Transformable{
	public:
		Mesh(vector<GLfloat> vertices, V3f position = { 0,0,0 }, V3f rotation = { 0,0,0 }, V3f scale = { 0,0,0 }, Texture* texture = nullptr, bool screenSpaceRendering = false, bool textureCoordinatesEnabled = true, vector<GLfloat> vertexColor = {});

		void render(Transform parentTransform);
		vector<GLfloat> vertices;
		Texture* meshTexture;
		V3f position = { 0,0,0 };
		V3f eulerRotation = { 0,0,0 };
		V3f scale = { 1,1,1 };
		vector<GLfloat> vertexColor = { 1.f,1.f,1.f };
	private:
		bool textureCoordinatesEnabled = true;
		bool screenSpaceRendering = false;
	};
}