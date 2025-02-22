#include "Mesh.h"

namespace CGEngine {
	Mesh::Mesh(VertexModel model, Transformation3D transformation, Texture* texture, Color vertexColor, RenderParameters renderParams) : model(model), transformation(transformation), meshTexture(texture), vertexColor(vertexColor), renderParameters(renderParams) { };

	void Mesh::render(Transform transform) {
		renderer.pullGL();
		bindTexture();
		renderer.bufferMeshData(model);
		renderer.renderMesh(model, transformation);
		renderer.commitGL();
	}

	void Mesh::bindTexture() {
		if (renderer.setGLWindowState(true)) {
			//Generate texture mipmaps and bind or clear
			if (meshTexture != nullptr) {
				(void)meshTexture->generateMipmap();
				Texture::bind(&(*meshTexture));
			}
			else {
				Texture::bind(nullptr);
			}
		}
		renderer.setGLWindowState(false);
	}
}