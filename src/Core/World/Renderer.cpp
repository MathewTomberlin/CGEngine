#include "Renderer.h"
#include "../Engine/Engine.h"

namespace CGEngine {
	void Renderer::setWindow(RenderWindow* window) {
		this->window = window;
	}
	void Renderer::initializeOpenGL() {
		if (currentCamera == nullptr) {
			currentCamera = new Camera({0,0,0},{0,0,0});
		}

		if (!setGLWindowState(true)) return;

		// Enable Z-buffer read and write (Disabling results in out-of-order polygon draws)
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClearDepth(1.f);

		// Enable Vertex and Texture Coordinate Arrays
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);

		//Enable color materials
		glEnable(GL_COLOR_MATERIAL);

		glEnable(GL_LIGHT0);
		GLfloat brightness = 4.f;
		GLfloat diffuseColor[4] = { 1 * brightness,1 * brightness,1 * brightness,1 * brightness };
		GLfloat ambientColor[4] = { 0,0,0,1 };
		GLfloat specularColor[4] = { 1 * brightness,1 * brightness,1 * brightness,1 * brightness };
		GLfloat lightPosition[4] = { 0,0,5,1 };
		GLfloat spotDirection[3] = { 0,0,-1 };
		GLfloat spotCutoff = 10.f;
		GLfloat spotExponent = 128.f;
		GLfloat spotConstantAttenuation = 2.f;
		GLfloat spotLinearAttenuation = 0.1f;
		GLfloat spotQuadraticAttenuation = 0.1f;
		glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseColor);
		glLightfv(GL_LIGHT0, GL_AMBIENT, ambientColor);
		glLightfv(GL_LIGHT0, GL_SPECULAR, specularColor);
		glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, spotDirection);
		glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, spotCutoff);
		glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, spotExponent);
		glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, spotConstantAttenuation);
		glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, spotLinearAttenuation);
		glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, spotQuadraticAttenuation);
		// Disable normal and color vertex components
		bool normalsEnabled = false;
		bool vertexColorsEnabled = false;
		if (!normalsEnabled) {
			glDisableClientState(GL_NORMAL_ARRAY);
		}
		else {
			glEnableClientState(GL_NORMAL_ARRAY);
		}

		if (!vertexColorsEnabled) {
			glDisableClientState(GL_COLOR_ARRAY);
		}
		else {
			glEnableClientState(GL_COLOR_ARRAY);
		}

		// Setup OPENGL lighting
		if (openGLSettings.lightingEnabled) {
			glEnable(GL_LIGHTING);
		} else {
			glDisable(GL_LIGHTING);
		}

		bool Texture2DEnabled = true;
		if (openGLSettings.texture2DEnabled) {
			glEnable(GL_TEXTURE_2D);
		} else {
			glDisable(GL_TEXTURE_2D);
		}

		// Setup OPENGL viewport within window
		FloatRect viewport = openGLSettings.viewport;
		GLsizei viewPositionX_px = viewport.position.x;
		GLsizei viewPositionY_px = viewport.position.y;
		float viewportSizeX = min(1.f, (float)window->getSize().y / window->getSize().x) * (float)window->getSize().x;
		float viewportSizeY = min(1.f, (float)window->getSize().x / window->getSize().y)* (float)window->getSize().y;
		int xOffset = ((int)window->getSize().x - viewportSizeX)/2;
		int yOffset = ((int)window->getSize().y - viewportSizeY)/2;

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		//This perspective is clipped by the near and far clip planes
		glFrustum(-1,1,-1,1, openGLSettings.nearClipPlane, openGLSettings.farClipPlane);
		//ViewPosition should allow the openGL viewport to be moved on the screen, x/yOffset centers the viewport, and viewportSizeXY squares it
		glViewport(viewPositionX_px+xOffset, viewPositionY_px+yOffset, viewportSizeX, viewportSizeY);
		if (!setGLWindowState(false)) return;
	}

	bool Renderer::setGLWindowState(bool state) {
		// Make the window no longer the active window for OpenGL calls
		bool success = window->setActive(state);
		if (!success) {
			std::cerr << "Failed to set window to " << ((state) ? "active" : "inactive") << std::endl;
		}
		return success;
	}

	bool Renderer::clearGL(GLbitfield mask) {
		if (!setGLWindowState(true)) return false;
		glClear(mask);
		return true;
	}

	void Renderer::commitGL() {
		window->pushGLStates();
	}

	void Renderer::pullGL() {
		window->popGLStates();
	}

	bool Renderer::processRender() {
		Body* root = world->getRoot();
		renderCamera();
		// Commit OpenGL State
		commitGL();
		// Clear the window
		window->clear();
		// Render SFML objects
		clear();
		root->render(*window, root->getTransform());
		render(window);
		// Pull OpenGL State
		pullGL();

		if (!setGLWindowState(false)) return false;
		window->display();
		return true;
	}

	void Renderer::add(Body* body, Transform transform) {
		renderOrder.push_back(body);
	}

	int Renderer::zMax() {
		return renderOrder.back()->zOrder;
	}

	int Renderer::zMin() {
		return (*renderOrder.begin())->zOrder;
	}

	vector<Body*> Renderer::getZBodies(int zIndex) {
		vector<Body*> bodies;
		bool found = false;
		for (int i = 0; i < renderOrder.size(); ++i) {
			if (renderOrder.at(i)->zOrder == zIndex) {
				found = true;
				bodies.push_back(renderOrder.at(i));
			}
			else if (found) {
				break;
			}
		}
		return bodies;
	}

	vector<Body*> Renderer::getLowerZBodies(int zIndex) {
		vector<Body*> bodies;
		bool found = false;
		for (int i = 0; i < renderOrder.size(); ++i) {
			if (renderOrder.at(i)->zOrder < zIndex) {
				bodies.push_back(renderOrder.at(i));
			}
			else {
				break;
			}
		}
		return bodies;
	}

	vector<Body*> Renderer::getHigherZBodies(int zIndex) {
		vector<Body*> bodies;
		bool found = false;
		for (int i = renderOrder.size() - 1; i >= 0; --i) {
			if (renderOrder.at(i)->zOrder > zIndex) {
				bodies.push_back(renderOrder.at(i));
			}
			else {
				break;
			}
		}
		return bodies;
	}

	void Renderer::clear() {
		renderOrder.clear();
	}
	
	void Renderer::sortZ() {
		sort(renderOrder.begin(), renderOrder.end(), [](Body* a, Body* b) { return (a->zOrder < b->zOrder); });
	}

	void Renderer::render(RenderTarget* window) {
		//Sort the render order by Body Z order
		sortZ();
		//Draw each body with its calculated transform
		for (auto iterator = renderOrder.begin(); iterator != renderOrder.end(); ++iterator) {
			if (world->isDeleted(*iterator)) continue;
			(*iterator)->onDraw(*window, (*iterator)->getGlobalTransform());
		}
	}

	void Renderer::renderCamera() {
		if (currentCamera != nullptr) {
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glFrustum(-1, 1, -1, 1, openGLSettings.nearClipPlane, openGLSettings.farClipPlane);
			glTranslatef(currentCamera->position.x, currentCamera->position.y, currentCamera->position.z);
			glRotatef(currentCamera->eulerRotation.x, 1, 0, 0);
			glRotatef(currentCamera->eulerRotation.y, 0, 1, 0);
			glRotatef(currentCamera->eulerRotation.z, 0, 0, 1);
		}
	}

	Camera* Renderer::getCurrentCamera() {
		return currentCamera;
	}

	void Renderer::setCurrentCamera(Camera* camera) {
		currentCamera = camera;
	}
}