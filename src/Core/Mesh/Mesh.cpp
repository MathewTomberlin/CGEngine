#include "Mesh.h"

namespace CGEngine {
	Mesh::Mesh(vector<GLfloat> vertices, vector<GLfloat> normals, V3f position, V3f rotation, V3f scale, Texture* texture, bool screenSpaceRendering, bool textureCoordinatesEnabled, vector<GLfloat> vertexColor, bool lightingEnabled, bool texture2dEnabled, bool normalsEnabled) : position(position), eulerRotation(rotation), scale(scale), vertices(vertices), meshTexture(texture), textureCoordinatesEnabled(textureCoordinatesEnabled), screenSpaceRendering(screenSpaceRendering), vertexColor(vertexColor), normals(normals), lightingEnabled(lightingEnabled), texture2dEnabled(texture2dEnabled), normalsEnabled(normalsEnabled) { };

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
			size_t vBufferSize = 3 + (textureCoordinatesEnabled ? 2 : 0);
			//Apply vertex buffer to vertex pointer
			glVertexPointer(3, GL_FLOAT, vBufferSize * sizeof(GLfloat), vertices.data());

			//Apply vertex buffer to texture coordinate pointer
			if (textureCoordinatesEnabled) {
				glTexCoordPointer(2, GL_FLOAT, vBufferSize * sizeof(GLfloat), vertices.data() + 3);
			}

			//Apply normals
			if (normalsEnabled) {
				glNormalPointer(GL_FLOAT, 0, normals.data());
			}

			//Apply vertex colors
			if (vertexColor.size()>0) {
				glColor3fv(vertexColor.data());
			} else {
				const GLfloat white[] = {1,1,1,1};
				glColor3fv(white);
			}
			glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

			//Get world xy position, xy right, z rotation, and xy scale from Body transform
			Vector2f worldPositionXY = world->getGlobalPosition(transform);
			Vector2f worldRightXYDir = world->getRight(transform);
			float worldAngleZ_Deg = world->getGlobalRotation(transform).asDegrees();
			Vector2f worldScaleXY = world->getGlobalScale(transform);

			//Get the position of the window View
			Vector2f viewPosition = screen->getCurrentView()->getTransform().transformPoint({ 0,0 });
			//If Screen Space Rendering is enabled, unset viewPosition
			if (screenSpaceRendering) {
				viewPosition = { 0,0 };
			}
			//Calculate the offset by current View size
			float viewScreenOffsetX = 2 / screen->getCurrentView()->getSize().x;
			float viewScreenOffsetY = 2 / screen->getCurrentView()->getSize().y;
			//Negate y position to match SFML y-axis orientation, add the view's position to convert from Screen Space, and offset by View size
			Vector2f globalPositionXY = V2f({ (worldPositionXY.x + position.x) *viewScreenOffsetX,(-worldPositionXY.y - position.y) *viewScreenOffsetY}) + viewPosition;
			
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			//Apply SFML global XY position to this Mesh's V3 position
			glTranslatef(globalPositionXY.x, globalPositionXY.y, position.z);
			//Scale by SFML scale on XY multiplied by Mesh's V3 scale
			glScalef(worldScaleXY.x * scale.x, worldScaleXY.y * scale.y, scale.z);
			//Rotate by eulerRotation on x
			glRotatef(eulerRotation.x, 1, 0, 0);
			//Rotate by eulerRotation on y
			glRotatef(eulerRotation.y, 0, 1, 0);
			//Rotate by eulerRotation + SFML rotation on z
			glRotatef(worldAngleZ_Deg + eulerRotation.z, 0, 0, 1);
			glEnable(GL_NORMALIZE);

			if (lightingEnabled && openGLSettings.lightingEnabled) {
				glEnable(GL_LIGHTING);
			} else {
				glDisable(GL_LIGHTING);
			}

			if (texture2dEnabled &&  openGLSettings.texture2DEnabled) {
				glEnable(GL_TEXTURE_2D);
			}
			else {
				glDisable(GL_TEXTURE_2D);
			}

			// Draw the cube
			glDrawArrays(GL_TRIANGLES, 0, (vertices.capacity() / 5));
			renderer.commitGL();
		}
	}
}