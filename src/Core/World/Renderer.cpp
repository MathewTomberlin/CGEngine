#include "../Engine/Engine.h"
#include "Renderer.h"
#include "../../Standard/Meshes/CommonVArrays.h"
namespace CGEngine {
	void Renderer::setWindow(RenderWindow* window) {
		this->window = window;
	}
	void Renderer::initializeOpenGL() {
		if (!setGLWindowState(true)) return;

		// Setup a camera with perspective projection
		GLfloat aspectRatio = static_cast<float>(window->getSize().x) / window->getSize().y;
		currentCamera = new Camera(aspectRatio);

		// Enable Z-buffer read and write
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		commitGL();
		if (!setGLWindowState(false)) return;
	}

	void Renderer::getModelData(Mesh* mesh) {
		if (setGLWindowState(true)) {
			VertexModel model = mesh->getModel();
			ShaderProgramPath shaderPath = mesh->getShaderProgramPaths();
			Material* meshMaterial = mesh->getMaterial();
			//Setup ModelData with drawCount, vertex buffer and array, and shader program
			ModelData data = ModelData(model.vertexCount, new Program(shaderPath), meshMaterial);
			glGenBuffers(1, &data.vbo);
			glGenVertexArrays(1, &data.vao);

			glBindVertexArray(data.vao);
			//Bind the vertex buffer and pass in the vertex data
			glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
			glBufferData(GL_ARRAY_BUFFER, model.dataSpan, model.vertices.data(), GL_STATIC_DRAW);
			auto stride = sizeof(GLfloat) * 8;
			auto textureCoordOffset = sizeof(GLfloat) * 3;
			auto normalOffset = sizeof(GLfloat) * 5;
			//Enable and prepare vertex and texture coordinate attribute arrays
			glEnableVertexAttribArray(data.shaders->attrib("position"));
			glVertexAttribPointer(data.shaders->attrib("position"), 3, GL_FLOAT, GL_FALSE, stride, NULL);
			glEnableVertexAttribArray(data.shaders->attrib("texCoord"));
			glVertexAttribPointer(data.shaders->attrib("texCoord"), 2, GL_FLOAT, GL_FALSE, stride, (void*)textureCoordOffset);
			glEnableVertexAttribArray(data.shaders->attrib("vertNormal"));
			glVertexAttribPointer(data.shaders->attrib("vertNormal"), 3, GL_FLOAT, GL_FALSE, stride, (void*)normalOffset);
			glBindVertexArray(0);

			mesh->setModelData(data);
			setGLWindowState(false);
		}
	}

	id_t Renderer::addLight(Light* light) {
		return lights.add(light);
	}

	void Renderer::removeLight(id_t lightId) {
		Light* light = lights.get(lightId);
		lights.remove(lightId);
		if (light != nullptr) {
			delete light;
		}
	}

	Light* Renderer::getLight(id_t lightId) {
		return lights.get(lightId);
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
		window->display();
		return true;
	}

	void Renderer::renderMesh(VertexModel model, Transformation3D transform, ModelData data) {
		//TODO: Apply scale
		glm::mat4 modelPos = glm::translate(glm::vec3(transform.position.x, transform.position.y, transform.position.z));
		glm::mat4 modelRotX = glm::rotate(transform.rotation.x, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 modelRotY = glm::rotate(transform.rotation.y, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 modelRotZ = glm::rotate(transform.rotation.z, glm::vec3(0.f, 0.f, 1.f));
		glm::mat4 modelScale = glm::scale(glm::vec3(transform.scale.x, transform.scale.y, transform.scale.z));
		glm::mat4 modelRotation = modelRotZ * modelRotY * modelRotX;
		glm::mat4 modelTransform = modelPos * modelRotation *modelScale;

		if (renderer.setGLWindowState(true)) {
			glBindVertexArray(data.vao);
			//Bind the shaders.
			data.shaders->use();

			//Set the uniforms for the shader to use
			Vector3f camPos = currentCamera->getPosition();
			glm::vec3 materialSpecularColor = { data.material->specularColor.r,data.material->specularColor.g,data.material->specularColor.b };
			glm::vec3 materialSurfaceColor = { data.material->diffuseColor.r,data.material->diffuseColor.g,data.material->diffuseColor.b };
			glm::vec2 materialDiffuseScale = { data.material->diffuseTextureUVScale.x,data.material->diffuseTextureUVScale.y };
			data.shaders->setUniform("camera", currentCamera->getMatrix());
			data.shaders->setUniform("model", modelTransform);
			data.shaders->setUniform("cameraPosition", { camPos.x,camPos.y,camPos.z });

			data.shaders->setUniform("material.diffuseColor", materialSurfaceColor);
			data.shaders->setUniform("material.diffuseTexture", 0);
			data.shaders->setUniform("material.diffuseTextureUVScale", materialDiffuseScale);
			data.shaders->setUniform("material.smoothnessFactor", data.material->shininess);
			data.shaders->setUniform("material.specularColor", materialSpecularColor);
			data.shaders->setUniform("material.opacity", data.material->opacity);
			data.shaders->setUniform("material.gammaCorrected", data.material->gammaCorrected);

			data.shaders->setUniform("lightCount", (int)lights.size());
			for (size_t i = 0; i < lights.size(); ++i) {
				glm::vec3 colorIntensities = { lights.get(i)->parameters.colorIntensities.x,lights.get(i)->parameters.colorIntensities.y,lights.get(i)->parameters.colorIntensities.z };
				glm::vec3 lightDirection = { lights.get(i)->parameters.lightDirection.x,lights.get(i)->parameters.lightDirection.y,lights.get(i)->parameters.lightDirection.z };
				data.shaders->setUniform(getUniformArrayPropertyName("lights",i,"position").c_str(), lights.get(i)->position);
				data.shaders->setUniform(getUniformArrayPropertyName("lights", i, "brightness").c_str(), lights.get(i)->parameters.brightness);
				data.shaders->setUniform(getUniformArrayPropertyName("lights", i, "intensities").c_str(), colorIntensities);
				data.shaders->setUniform(getUniformArrayPropertyName("lights", i, "attenuation").c_str(), lights.get(i)->parameters.attenuation);
				data.shaders->setUniform(getUniformArrayPropertyName("lights", i, "ambiance").c_str(), lights.get(i)->parameters.ambiance);
				data.shaders->setUniform(getUniformArrayPropertyName("lights", i, "coneAngle").c_str(), lights.get(i)->parameters.coneAngle);
				data.shaders->setUniform(getUniformArrayPropertyName("lights", i, "lightDirection").c_str(), lightDirection);
			}

			// Draw the cube
			glDrawArrays(GL_TRIANGLES, 0, model.vertices.size()/5);

			// Unbind varray, shaders, and texture
			glBindVertexArray(0);
			data.shaders->stop();
		}
		if (renderer.setGLWindowState(false));
	}

	string Renderer::getUniformArrayPropertyName(string arrayName, int index, string propertyName) {
		std::ostringstream ss;
		ss << arrayName << "[" << index << "]." << propertyName;
		return ss.str();
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