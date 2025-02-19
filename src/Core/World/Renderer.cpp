#include "Renderer.h"
#include "../Engine/Engine.h"

namespace CGEngine {
	void Renderer::setWindow(RenderWindow* window) {
		this->window = window;
	}
	void Renderer::initializeOpenGL() {
		if (!setGLWindowState(true)) return;

		// Enable Z-buffer read and write (Disabling results in out-of-order polygon draws)
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClearDepth(1.f);

		// Enable Vertex and Texture Coordinate Arrays
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

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
		}
		else {
			glDisable(GL_LIGHTING);
		}

		bool Texture2DEnabled = true;
		if (openGLSettings.texture2DEnabled) {
			glEnable(GL_TEXTURE_2D);
		}
		else {
			glDisable(GL_TEXTURE_2D);
		}

		// Setup OPENGL viewport within window
		FloatRect viewport = openGLSettings.viewport;
		GLsizei viewPositionX_px = viewport.position.x;
		GLsizei viewPositionY_px = viewport.position.y;
		float viewportSizeY = min(1.f, (float)window->getSize().x / window->getSize().y)* (float)window->getSize().y;
		float viewportSizeX = min(1.f, (float)window->getSize().y / window->getSize().x)* (float)window->getSize().x;
		float minDim = min(window->getSize().x, window->getSize().y);
		GLsizei viewSizeY_px = static_cast<GLsizei>(minDim * viewport.size.y);
		int yOffset = ((int)window->getSize().y - viewportSizeY);
		int xOffset = ((int)window->getSize().x - viewportSizeX);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glFrustum(-1,1,-1,1, openGLSettings.nearClipPlane, openGLSettings.farClipPlane);
		glViewport(viewPositionX_px+(xOffset/2), viewPositionY_px+(yOffset/2), viewportSizeX, viewportSizeY);
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
}