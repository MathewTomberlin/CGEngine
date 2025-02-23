#include "../Engine/Engine.h"
#include "Renderer.h"
#include "../../Standard/Meshes/CommonVArrays.h"
namespace CGEngine {
	void Renderer::setWindow(RenderWindow* window) {
		this->window = window;
	}
	void Renderer::initializeOpenGL() {
		if (!setGLWindowState(true)) return;

		program = new Program("shaders/StdVertexShader.txt", "shaders/StdFragShader.txt");
		// Setup a camera with perspective projection
		GLfloat aspectRatio = static_cast<float>(window->getSize().x) / window->getSize().y;
		currentCamera = new Camera(aspectRatio);

		// Enable Z-buffer read and write
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		auto stride = sizeof(GLfloat) * 5;
		auto textureCoordOffset = sizeof(GLfloat) * 3;
		//Generate and bind vertex array and vertex buffer
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vertexVBO);
		glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);
		//Enable and prepare vertex and texture coordinate attribute arrays
		glEnableVertexAttribArray(program->attrib("position"));
		glVertexAttribPointer(program->attrib("position"), 3, GL_FLOAT, GL_FALSE, stride, 0);
		glEnableVertexAttribArray(program->attrib("texCoord"));
		glVertexAttribPointer(program->attrib("texCoord"), 2, GL_FLOAT, GL_FALSE, stride, (void*)textureCoordOffset);
		//Generate and bind the index buffer
		glGenBuffers(1, &indexVBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVBO);
		//Make sure to bind the vertex array to null if you wish to define more objects.
		glBindVertexArray(0);
		commitGL();
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
		//Clear render order
		clear();
		//Collect Bodies to render
		Body* root = world->getRoot();
		root->render(*window, root->getTransform());
		//Sort and render Meshes and SFML entities
		render(window);
		//Reset OpenGL parameters
		if (setGLWindowState(true)) {
			glBindVertexArray(0);
			glUseProgram(0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		if (!setGLWindowState(false));
		window->display();
		return true;
	}

	void Renderer::renderMesh(VertexModel model, Transformation3D transform) {
		//TODO: Apply scale
		glm::mat4 modelPos = glm::translate(glm::vec3(transform.position.x, transform.position.y, transform.position.z));
		glm::mat4 modelRotX = glm::rotate(transform.rotation.x, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 modelRotY = glm::rotate(transform.rotation.y, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 modelRotZ = glm::rotate(transform.rotation.z, glm::vec3(0.f, 0.f, 1.f));
		glm::mat4 modelRotation = modelRotZ * modelRotY * modelRotX;
		glm::mat4 modelTransform = modelPos * modelRotation;

		if (renderer.setGLWindowState(true)) {
			//Bind the shaders.
			glUseProgram(program->getObjectId());

			//Set the uniforms for the shader to use
			program->setUniform("camera", currentCamera->getTransform());
			program->setUniform("model", modelTransform);
			program->setUniform("projection", currentCamera->getProjection());
			program->setUniform("texture", 0);

			// Draw the cube
			glDrawElements(GL_TRIANGLES, model.indices.size(), GL_UNSIGNED_INT, 0);

			// Unbind varray, program, and texture
			glBindVertexArray(0);
			glUseProgram(0);
		}
		if (renderer.setGLWindowState(false));
	}

	void Renderer::bufferMeshData(VertexModel model) {
		//Apply the incoming model data
		if (renderer.setGLWindowState(true)) {
			glBindVertexArray(renderer.vao);
			//Apply model vertices and vertex texture coordinate data
			glBufferData(GL_ARRAY_BUFFER, model.vertexComponentSpan, model.vertices.data(), GL_STATIC_DRAW);
			//Apply model vertex face indices
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.vertexSpan, model.indices.data(), GL_STATIC_DRAW);
		}
		renderer.setGLWindowState(false);
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

	Camera* Renderer::getCurrentCamera() {
		return currentCamera;
	}

	void Renderer::setCurrentCamera(Camera* camera) {
		currentCamera = camera;
	}

	GLenum Renderer::initGlew() {
		// Initialise GLEW for the extended functions.
		glewExperimental = GL_TRUE;
		GLenum state = glewInit();
		if (state != GLEW_OK) {
			cout << "GLEW not ok" << "\n";
		} else {
			cout << "GLEW initialized" << "\n";
		}
		return state;
	}
}