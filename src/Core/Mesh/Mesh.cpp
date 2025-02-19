#include "Mesh.h"

namespace CGEngine {
	Mesh::Mesh(vector<GLfloat> vertices, Texture* texture, float depth, bool screenSpaceRendering, bool textureCoordinatesEnabled) : vertices(vertices), meshTexture(texture), textureCoordinatesEnabled(textureCoordinatesEnabled), depth(depth), screenSpaceRendering(screenSpaceRendering) { };

	void Mesh::render(Transform transform) {
		if (renderer.setGLWindowState(true)) {
			renderer.pullGL();
			(void)meshTexture->generateMipmap();
			Texture::bind(&(*meshTexture));

			// Enable vertex component
			glVertexPointer(3, GL_FLOAT, 5 * sizeof(GLfloat), vertices.data());

			//Enable texture coordinate component if enabled
			if (textureCoordinatesEnabled) {
				glTexCoordPointer(2, GL_FLOAT, 5 * sizeof(GLfloat), vertices.data() + 3);
			}

			//Get world xy position, xy right, z rotation, and xy scale from Body transform
			Vector2f worldPositionXY = transform.transformPoint({ 0,0 });
			Vector2f worldRightXYDir = (transform.transformPoint({ 1,0 }) - worldPositionXY);
			if (worldRightXYDir.lengthSquared() > 0.001f) worldRightXYDir = worldRightXYDir.normalized();
			float worldAngleZ_Deg = atan2(worldRightXYDir.y, worldRightXYDir.x) * 180.0f / (float)M_PI;
			if (worldAngleZ_Deg < 0) worldAngleZ_Deg += 360.0f;
			Angle worldAngleZ = degrees(worldAngleZ_Deg);
			Transform unrotatedTransform = transform.rotate(-worldAngleZ);
			Vector2f worldScaleXY = (unrotatedTransform.transformPoint({ 1,1 }) - unrotatedTransform.transformPoint({ 0,0 }));

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
			Vector2f globalPositionXY = V2f({ worldPositionXY.x*viewScreenOffsetX,-worldPositionXY.y*viewScreenOffsetY}) + viewPosition;
			//Apply the SFML XY transformations along with height and -depth
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			//TODO: Replace depth Mesh Vector3 position (add position xy to globalPositonXY)
			glTranslatef(globalPositionXY.x, globalPositionXY.y, -depth);
			//TODO: Apply X and Y rotations
			glRotatef(worldAngleZ_Deg, 0, 0, 1);
			//TODO: Replace height Mesh Vector3 scale (multiply position xy to globalPositonXY)
			glScalef(worldScaleXY.x, worldScaleXY.y, height);

			// Draw the cube
			glDrawArrays(GL_TRIANGLES, 0, 36);
			renderer.commitGL();
		}
	}
}