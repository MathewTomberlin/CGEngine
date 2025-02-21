#pragma once

#include <SFML/OpenGL.hpp>
#include "../Types/V3.h"
#include "../Engine/Engine.h"

namespace CGEngine {
	struct RenderParameters {
		RenderParameters(bool lighting = true, bool textures = true, bool screenSpace = false, bool normals = true, bool textureCoords = true) :lightingEnabled(lighting),texture2dEnabled(textures),screenSpaceRendering(screenSpace),normalsEnabled(normals), textureCoordinatesEnabled(textureCoords){};

		bool lightingEnabled = true;
		bool texture2dEnabled = true;
		bool screenSpaceRendering = false;
		bool normalsEnabled = true;
		bool textureCoordinatesEnabled = true;
	};

	struct VertexModel {
		VertexModel(vector<GLfloat> vertices, vector<GLfloat> normals = {}) :vertices(vertices), normals(normals) {};

		vector<GLfloat> vertices;
		vector<GLfloat> normals;
	};

	struct Transformation3D {
		Transformation3D(Vector3f position = Vector3f()) :position(position), rotation(Vector3f()), scale(Vector3f()) { };
		Transformation3D(Vector3f position, Vector3f rotation, Vector3f scale) :position(position), rotation(rotation), scale(scale) { };
		Transformation3D(Vector3f position, Vector3f scale) :position(position), rotation(Vector3f()), scale(scale) { };

		Vector3f position;
		Vector3f rotation;
		Vector3f scale;
	};

	class Mesh : public Transformable{
	public:
		Mesh(VertexModel model, Transformation3D transformation = Transformation3D(), Texture* texture = nullptr, Color vertexColor = {}, RenderParameters renderParams = RenderParameters());

		void render(Transform parentTransform);
		VertexModel model;
		Texture* meshTexture;
		Transformation3D transformation;
		Color vertexColor = Color::White;
	private:
		RenderParameters renderParameters;
	};
}