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

	class Mesh : public IResource, public Transformable{
	public:
		Mesh(MeshData* model, Transformation3D transformation = Transformation3D(), vector<Material*> materials = { new Material(SurfaceParameters()) }, RenderParameters renderParams = RenderParameters(), string importPath = "");
		Mesh(string importPath, Transformation3D transformation = Transformation3D(), vector<Material*> materials = { new Material(SurfaceParameters()) }, RenderParameters renderParams = RenderParameters());

		void render(Transform parentTransform);
		void bindTexture(Texture* texture);
		void setPosition(Vector3f pos);
		void move(Vector3f delta);
		void setRotation(Vector3f rot);
		void rotate(Vector3f delta);
		void setScale(Vector3f scale);
		void scale(Vector3f delta);
		MeshData* getMeshData();
		void setMeshData(MeshData* model);
		vector<Material*> getMaterials();
		id_t addMaterial(Material* material);
		void clearMaterials();
		string getMeshName() const;
		string getSourcePath() const;
		optional<id_t> getModel() const { return modelId; }
		void setModel(optional<id_t> modelId) { this->modelId = modelId; }
		Body* getBody() const { return body; }
		void setBody(Body* body) { this->body = body; }
		// Add new method to get combined transform
		glm::mat4 getModelMatrix() const {
			glm::mat4 modelPos = glm::translate(glm::vec3(transformation.position.x, transformation.position.y, transformation.position.z));
			glm::mat4 modelRotX = glm::rotate(degrees(transformation.rotation.x).asRadians(), glm::vec3(1.f, 0.f, 0.f));
			glm::mat4 modelRotY = glm::rotate(degrees(transformation.rotation.y).asRadians(), glm::vec3(0.f, 1.f, 0.f));
			glm::mat4 modelRotZ = glm::rotate(degrees(transformation.rotation.z).asRadians(), glm::vec3(0.f, 0.f, 1.f));
			glm::mat4 modelRotation = modelRotZ * modelRotY * modelRotX;
			glm::mat4 modelScale = glm::scale(glm::vec3(transformation.scale.x, transformation.scale.y, transformation.scale.z));
			return modelPos * modelRotation * modelScale;
		}
		bool isValid() const override {
			return meshData != nullptr && !meshData->vertices.empty();
		}
	private:
		string importPath;
		optional<id_t> modelId;
		Body* body = nullptr;
		MeshData* meshData;
		Transformation3D transformation;
		RenderParameters renderParameters;
		vector<Material*> materials;
	};
}