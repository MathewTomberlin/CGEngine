#include "Mesh.h"

namespace CGEngine {
	Mesh::Mesh(vector<GLfloat> vertices, Texture* texture, bool textureCoordinatesEnabled, float depth) : vertices(vertices), meshTexture(texture), textureCoordinatesEnabled(textureCoordinatesEnabled) {
		this->depth = depth;
		position = { 0,0 };
	};

	void Mesh::render(Transform transform) {
		if (renderer.setGLWindowState(true)) {
			(void)meshTexture->generateMipmap();
			Texture::bind(&(*meshTexture));

			// Enable vertex component
			glVertexPointer(3, GL_FLOAT, 5 * sizeof(GLfloat), vertices.data());

			//Enable texture coordinate component if enabled
			if (textureCoordinatesEnabled) {
				glTexCoordPointer(2, GL_FLOAT, 5 * sizeof(GLfloat), vertices.data() + 3);
			}

			//Get world position, right, z rotation, and scale from Body transform
			Vector2f worldPosition = transform.transformPoint({ 0,0 });
			Vector2f rightWorldPosition = transform.transformPoint({ 1,0 });
			Vector2f worldRightDir = (rightWorldPosition - worldPosition);
			if (worldRightDir.lengthSquared() > 0.001f) worldRightDir = worldRightDir.normalized();
			float worldAngle_Deg = atan2(worldRightDir.y, worldRightDir.x) * 180.0f / (float)M_PI;
			if (worldAngle_Deg < 0) worldAngle_Deg += 360.0f;
			Angle worldAngle = degrees(worldAngle_Deg);
			Transform rotatedTransform = transform.rotate(-worldAngle);
			Vector2f worldScaleA = rotatedTransform.transformPoint({ 0,0 });
			Vector2f worldScaleB = rotatedTransform.transformPoint({ 1,1 });
			Vector2f worldScale = (worldScaleB - worldScaleA);
			//Get the position of the window View
			Vector2f viewPosition = screen->getCurrentView()->getTransform().transformPoint({ 0,0 });
			//Get the ratio of window size
			float windowSizeRatio = screen->getSize().x / screen->getSize().y;
			//Multiply world positon + view position by windowSizeRatio for x or 1/windowSizeRatio for y or use 1 for whichever is less than 1
			//TODO: Test if windowSizeRatio should actually be OpenGL ViewportSizeRatio or window View ViewportSizeRatio or some combination of them
			//		I suspect it's actually based on the OpenGL ViewportSizeRatio since it affects perspective
			Vector2f viewOffsetWorldPosition = { (worldPosition.x + viewPosition.x) * max(windowSizeRatio,1.f) * depth,(worldPosition.y + viewPosition.y) * max((1/windowSizeRatio),1.f) * depth};

			// Apply some transformations
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glTranslatef(viewOffsetWorldPosition.x, viewOffsetWorldPosition.y, -depth);
			glRotatef(worldAngle_Deg, 0, 0, 1);
			glScalef(worldScale.x, worldScale.y, height);

			// Draw the cube
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
	}
}