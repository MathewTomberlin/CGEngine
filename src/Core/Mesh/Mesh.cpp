#include "Mesh.h"

namespace CGEngine {
	Mesh::Mesh(VertexModel model, Transformation3D transformation, Texture* texture, Color vertexColor, RenderParameters renderParams) : model(model), transformation(transformation), meshTexture(texture), vertexColor(vertexColor), renderParameters(renderParams) { };

	void Mesh::render(Transform transform) {
		if (renderer.setGLWindowState(true)) {
			renderer.pullGL();
			
			//Generate mesh texture mips and binds
			if (meshTexture != nullptr) {
				(void)meshTexture->generateMipmap();
				Texture::bind(&(*meshTexture));
			} else {
				Texture::bind(nullptr);
			}
			
			//Size of vertex buffer determined by presence of texture coordinates
			size_t vBufferSize = 3 + (renderParameters.textureCoordinatesEnabled ? 2 : 0);
			//Apply vertex buffer to vertex pointer
			glVertexPointer(3, GL_FLOAT, vBufferSize * sizeof(GLfloat), model.vertices.data());

			//Apply vertex buffer to texture coordinate pointer
			if (renderParameters.textureCoordinatesEnabled) {
				glTexCoordPointer(2, GL_FLOAT, vBufferSize * sizeof(GLfloat), model.vertices.data() + 3);
			}

			//Apply normals
			if (renderParameters.normalsEnabled) {
				glNormalPointer(GL_FLOAT, 0, model.normals.data());
			}

			//Apply vertex colors
			const GLfloat color[] = { vertexColor.r,vertexColor.g,vertexColor.b,vertexColor.a };
			glColor3fv(color);
			
			//Apply lit material
			glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

			//Get world xy position, xy right, z rotation, and xy scale from Body transform
			Vector2f worldPositionXY = world->getGlobalPosition(transform);
			Vector2f worldRightXYDir = world->getRight(transform);
			float worldAngleZ_Deg = world->getGlobalRotation(transform).asDegrees();
			Vector2f worldScaleXY = world->getGlobalScale(transform);

			//Get the position of the window View
			Vector2f viewPosition = screen->getCurrentView()->getTransform().transformPoint({ 0,0 });
			//If Screen Space Rendering is enabled, unset viewPosition
			if (renderParameters.screenSpaceRendering) {
				viewPosition = { 0,0 };
			}
			//Calculate the offset by current View size
			float viewScreenOffsetX = 2 / screen->getCurrentView()->getSize().x;
			float viewScreenOffsetY = 2 / screen->getCurrentView()->getSize().y;
			//Negate y position to match SFML y-axis orientation, add the view's position to convert from Screen Space, and offset by View size
			Vector2f globalPositionXY = V2f({ (worldPositionXY.x + transformation.position.x) *viewScreenOffsetX,(-worldPositionXY.y - transformation.position.y) *viewScreenOffsetY}) + viewPosition;
			
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			//Apply SFML global XY position to this Mesh's V3 position
			glTranslatef(globalPositionXY.x, globalPositionXY.y, transformation.position.z);
			//Scale by SFML scale on XY multiplied by Mesh's V3 scale
			glScalef(worldScaleXY.x * transformation.scale.x, worldScaleXY.y * transformation.scale.y, transformation.scale.z);
			//Rotate by eulerRotation on x
			glRotatef(transformation.rotation.x, 1, 0, 0);
			//Rotate by eulerRotation on y
			glRotatef(transformation.rotation.y, 0, 1, 0);
			//Rotate by eulerRotation + SFML rotation on z
			glRotatef(worldAngleZ_Deg + transformation.rotation.z, 0, 0, 1);
			glEnable(GL_NORMALIZE);

			if (renderParameters.lightingEnabled && openGLSettings.lightingEnabled) {
				glEnable(GL_LIGHTING);
			} else {
				glDisable(GL_LIGHTING);
			}

			if (renderParameters.texture2dEnabled &&  openGLSettings.texture2DEnabled) {
				glEnable(GL_TEXTURE_2D);
			}
			else {
				glDisable(GL_TEXTURE_2D);
			}

			// Draw the cube
			glDrawArrays(GL_TRIANGLES, 0, (model.vertices.capacity() / vBufferSize));
			renderer.commitGL();
		}
	}
}