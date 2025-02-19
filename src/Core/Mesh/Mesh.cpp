#include "Mesh.h"

namespace CGEngine {
	Mesh::Mesh(vector<GLfloat> vertices, V3f position, V3f rotation, V3f scale, Texture* texture, bool screenSpaceRendering, bool textureCoordinatesEnabled) : position(position), eulerRotation(rotation), scale(scale), vertices(vertices), meshTexture(texture), textureCoordinatesEnabled(textureCoordinatesEnabled), screenSpaceRendering(screenSpaceRendering) { };

	void Mesh::render(Transform transform) {
		if (renderer.setGLWindowState(true)) {
			renderer.pullGL();
			if (meshTexture != nullptr) {
				(void)meshTexture->generateMipmap();
				Texture::bind(&(*meshTexture));
			} else {
				Texture::bind(nullptr);
			}

			// Enable vertex component
			glVertexPointer(3, GL_FLOAT, 5 * sizeof(GLfloat), vertices.data());

			//Enable texture coordinate component if enabled
			if (textureCoordinatesEnabled) {
				glTexCoordPointer(2, GL_FLOAT, 5 * sizeof(GLfloat), vertices.data() + 3);
			}

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

			// Draw the cube
			glDrawArrays(GL_TRIANGLES, 0, 36);
			renderer.commitGL();
		}
	}
}