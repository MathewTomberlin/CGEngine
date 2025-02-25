#pragma once

#include "../Engine/Engine.h"
#include "../Material/Material.h"

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
		Mesh(VertexModel model, Transformation3D transformation = Transformation3D(), Material* material = new Material(), RenderParameters renderParams = RenderParameters());

		void render(Transform parentTransform);
		void bindTexture();
		void setPosition(Vector3f pos);
		void move(Vector3f delta);
		void setRotation(Vector3f rot);
		void rotate(Vector3f delta);
		void setScale(Vector3f scale);
		void scale(Vector3f delta);
		void setModelData(ModelData data);
		VertexModel getModel();
		Material* getMaterial();
	private:
		VertexModel model;
		ModelData modelData;
		Texture* meshTexture = nullptr;
		Transformation3D transformation;
		RenderParameters renderParameters;
		Material* material;
	};
}