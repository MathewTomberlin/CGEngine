#pragma once

#include "../Engine/Engine.h"
#include "../Material/Material.h"
#include "../Animation/Animator.h"

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
		Mesh(MeshData* model, Transformation3D transformation = Transformation3D(), vector<Material*> materials = { new Material(SurfaceParameters()) }, RenderParameters renderParams = RenderParameters(), string importPath = "", bool skeletalMesh = false);
		Mesh(string importPath, Transformation3D transformation = Transformation3D(), bool skeletalMesh = false, vector<Material*> materials = { new Material(SurfaceParameters()) }, RenderParameters renderParams = RenderParameters()) : Mesh(new MeshData(), transformation, materials, renderParams, importPath, skeletalMesh) {};

		void render(Transform parentTransform);
		void bindTexture(Texture* texture);
		void setPosition(Vector3f pos);
		void move(Vector3f delta);
		void setRotation(Vector3f rot);
		void rotate(Vector3f delta);
		void setScale(Vector3f scale);
		void scale(Vector3f delta);
		void setImportPath(string path);
		string getImportPath();
		MeshData* getMeshData();
		void setMeshData(MeshData* model);
		vector<Material*> getMaterials();
		id_t addMaterial(Material* material);
		void clearMaterials();
		void setAnimator(Animator* animator);
		Animator* getAnimator();
	private:
		string importPath;
		MeshData* meshData;
		Transformation3D transformation;
		RenderParameters renderParameters;
		vector<Material*> materials;
		Animator* animator = nullptr;
	};
}