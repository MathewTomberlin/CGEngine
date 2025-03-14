#include "Mesh.h"

namespace CGEngine {
	Mesh::Mesh(VertexModel model, Transformation3D transformation, vector<Material*> materials, RenderParameters renderParams) : model(model), transformation(transformation), renderParameters(renderParams), materials(materials) {
		renderer.getModelData(this);
	};

	void Mesh::setModelData(ModelData data) {
		modelData = data;
	}

	VertexModel Mesh::getModel() {
		return model;
	}

	void Mesh::setModel(VertexModel model) {
		this->model = model;
	}

	void Mesh::render(Transform transform) {
		renderer.pullGL();

		//Combine SFML entity transform components with 3D transformation components
		Vector2f position2d = world->getGlobalPosition(transform);
		Vector3f position = { position2d.x + transformation.position.x,position2d.y + transformation.position.y,transformation.position.z };

		Angle rotation2d = world->getGlobalRotation(transform);
		Vector3f rotation = { transformation.rotation.x,transformation.rotation.y, rotation2d.asDegrees()+transformation.rotation.z, };

		Vector2f scale2d = world->getGlobalScale(transform);
		Vector3f scale = { scale2d.x * transformation.scale.x,scale2d.y * transformation.scale.y, transformation.scale.z };
		Transformation3D combinedTransformation = Transformation3D(position, rotation, scale);

		renderer.renderMesh(model, combinedTransformation, modelData);
		renderer.commitGL();
	}

	void Mesh::bindTexture(Texture* texture) {
		if (renderer.setGLWindowState(true)) {
			//Generate texture mipmaps and bind or clear
			if (texture != nullptr) {
				(void)texture->generateMipmap();
				Texture::bind(&(*texture));
			}
			else {
				Texture::bind(nullptr);
			}
			renderer.setGLWindowState(false);
		}
	}

	void Mesh::setPosition(Vector3f pos) {
		transformation.position = pos;
	}
	void Mesh::move(Vector3f delta) {
		transformation.position += delta;
	}

	void Mesh::setRotation(Vector3f rot) {
		transformation.rotation = rot;
	}

	void Mesh::rotate(Vector3f delta) {
		transformation.rotation += delta;
	}

	void Mesh::setScale(Vector3f scale) {
		transformation.scale = scale;
	}

	void Mesh::scale(Vector3f delta) {
		transformation.scale += delta;
	}

	vector<Material*> Mesh::getMaterials() {
		return materials;
	}
}