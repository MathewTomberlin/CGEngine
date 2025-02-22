#pragma once

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

	class Mesh : public Transformable{
	public:
		Mesh(VertexModel model, Transformation3D transformation = Transformation3D(), Texture* texture = nullptr, Color vertexColor = {}, RenderParameters renderParams = RenderParameters());

		void render(Transform parentTransform);
		void bindTexture();
	private:
		VertexModel model;
		Texture* meshTexture;
		Transformation3D transformation;
		Color vertexColor = Color::White;
		RenderParameters renderParameters;
	};
}