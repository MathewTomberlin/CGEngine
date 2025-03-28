#include "../Engine/Engine.h"
#include "Renderer.h"
#include "../../Standard/Models/CommonModels.h"
#include "../Animation/Animator.h"
#include "../Mesh/Model.h"

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
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		commitGL();
		if (!setGLWindowState(false)) return;
	}

	void Renderer::getModelData(Mesh* mesh) {
		if (setGLWindowState(true)) {
			MeshData* meshData = mesh->getMeshData();
			if (!meshData) return;
			vector<Material*> material = mesh->getMaterials();
			Material* renderMaterial = world->getMaterial(fallbackMaterialId);
			if (material.size() > 0 && material[0] && material[0]->getProgram()) {
				renderMaterial = material[0];
			}

			Program* program = renderMaterial->getProgram();
			glGenBuffers(1, & meshData->vbo);
			glGenBuffers(1, &meshData->ebo);
			glGenVertexArrays(1, &meshData->vao);

			glBindVertexArray(meshData->vao);
			//Bind the vertex buffer and pass in the vertex data
			glBindBuffer(GL_ARRAY_BUFFER,  meshData->vbo);
			glBufferData(GL_ARRAY_BUFFER,  meshData->vertices.size()*sizeof(VertexData), meshData->vertices.data(), GL_STATIC_DRAW);

			//Bind and buffert the element buffer, if the model has indices
			if (meshData->indices.size() > 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,  meshData->ebo);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER,  meshData->indices.size()*sizeof(unsigned int), meshData->indices.data(), GL_STATIC_DRAW);
			}

			//Enable and prepare vertex, texture coordinate, normal, and textureId attribute arrays
			glEnableVertexAttribArray(program->attrib("position"));
			glVertexAttribPointer(program->attrib("position"), 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), NULL);
			glEnableVertexAttribArray(program->attrib("texCoord"));
			glVertexAttribPointer(program->attrib("texCoord"), 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData,texCoord));
			glEnableVertexAttribArray(program->attrib("vertNormal"));
			glVertexAttribPointer(program->attrib("vertNormal"), 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
			glEnableVertexAttribArray(program->attrib("materialId"));
			glVertexAttribPointer(program->attrib("materialId"), 1, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, materialId));
			if (!meshData->bones.empty()) {
				glEnableVertexAttribArray(program->attrib("boneIds"));
				glVertexAttribIPointer(program->attrib("boneIds"), 4, GL_INT, sizeof(VertexData), (void*)offsetof(VertexData, boneIds));
				glEnableVertexAttribArray(program->attrib("weights"));
				glVertexAttribPointer(program->attrib("weights"), 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, weights));
			}
			glBindVertexArray(0);

			mesh->setMeshData(meshData);
			setGLWindowState(false);
		}
	}

	ImportResult Renderer::import(string path) {
		return importer->importModel(path);
	}

	Material* Renderer::getFallbackMaterial() {
		return world->getMaterial(fallbackMaterialId);
	}

	void Renderer::updateModelData(Mesh* mesh) {
		if (setGLWindowState(true)) {
			MeshData* meshData = mesh->getMeshData();
			vector<Material*> material = mesh->getMaterials();
			Material* renderMaterial = world->getMaterial(fallbackMaterialId);
			if (material.size() > 0 && material[0] && material[0]->getProgram()) {
				renderMaterial = material[0];
			}

			glBindVertexArray(meshData->vao);
			//Bind the vertex buffer and pass in the vertex data
			glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo);
			glBufferData(GL_ARRAY_BUFFER, meshData->vertices.size() * sizeof(VertexData), meshData->vertices.data(), GL_STATIC_DRAW);

			//Bind and buffert the element buffer, if the model has indices
			if (meshData->indices.size() > 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData->ebo);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData->indices.size() * sizeof(unsigned int), meshData->indices.data(), GL_STATIC_DRAW);
			}
			glBindVertexArray(0);

			mesh->setMeshData(meshData);
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
			cerr << "Failed to set window to " << ((state) ? "active" : "inactive") << endl;
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
		endFrame();
		window->display();
		return true;
	}

	void Renderer::renderMesh(Mesh* mesh, MeshData* model, Transformation3D transform) {
		if (!mesh || !model || model->vertices.empty()) return;

		Model* meshModel = mesh->getModel();

		// Get the body that owns this mesh
		Body* meshBody = nullptr;
		world->bodies.forEach([&mesh, &meshBody](Body* body) {
			if (body->get<Mesh*>() == mesh) {
				meshBody = body;
				return;
			}
		});
		if (!meshBody) return;

		// Get combined transform from entire hierarchy
		glm::mat4 combinedTransform = getCombinedModelMatrix(meshBody);

		if (renderer.setGLWindowState(true)) {
			// Only proceed with mesh rendering if we have mesh data
			if (model && model->vertices.size() > 0) {
				glBindVertexArray(model->vao);
				boundTextures = 0;
				
				// Get materials from Model instead of Mesh
				vector<Material*> modelMaterials = mesh->getMaterials();

				// Ensure at least one material (fallback)
				if (modelMaterials.empty()) {
					cout << "WARNING: No model materials in renderer, falling back!\n";
					modelMaterials.push_back(world->getMaterial(fallbackMaterialId));
				}
				Material* renderMaterial = modelMaterials.at(0);
				Animator* animator = mesh->getAnimator();
				if (meshModel && animator) {
					// Only update animation once per model per frame
					if (updatedModels.find(meshModel) == updatedModels.end()) {
						mesh->getAnimator()->updateAnimation(time.getDeltaSec());
						updatedModels.insert(meshModel);
					}
				}
				Program* program = renderMaterial->getProgram();
				//Bind the shaders.
				program->use();
				//Set the uniforms for the shader to use
				Vector3f camPos = currentCamera->getPosition();
				program->setUniform("model", combinedTransform);
				program->setUniform("camera", currentCamera->getMatrix());
				program->setUniform("cameraPosition", { camPos.x,camPos.y,camPos.z });
				program->setUniform("timeSec", time.getElapsedSec());
				if (animator) {
					vector<glm::mat4> transforms = animator->getBoneMatrices();
					for (int i = 0; i < transforms.size(); ++i) {
						program->setUniform(getUniformArrayIndexName("boneMatrices", i).c_str(), transforms[i]);
					}
				}
				for (int i = 0; i < modelMaterials.size(); ++i) {
					setMaterialUniforms(modelMaterials.at(i), program, i);
				}
				program->setUniform("lightCount", (int)lights.size());
				for (size_t i = 0; i < lights.size(); ++i) {
					setLightUniforms(lights.get(i), i, program);
				}
				// Draw the cube
				if (model->indices.size()) {
					glDrawElements(GL_TRIANGLES, model->indices.size(), GL_UNSIGNED_INT, 0);
				} else {
					glDrawArrays(GL_TRIANGLES, 0, model->vertices.size() / 5);
				}

				// Unbind varray, shaders, and texture
				glBindVertexArray(0);
				glActiveTexture(GL_TEXTURE0);
				program->stop();
			}
		}
		if (renderer.setGLWindowState(false));
	}

	void Renderer::endFrame() {
		updatedModels.clear();
	}

	glm::mat4 Renderer::getCombinedModelMatrix(Body* body) {
		glm::mat4 localTransform = glm::mat4(1.0f);

		// Get mesh transform if available
		Mesh* mesh = body->get<Mesh*>();
		if (mesh) {
			localTransform = mesh->getModelMatrix();
		}

		// Combine with parent transform
		if (body->parent && body->parent != world->getRoot()) {
			return getCombinedModelMatrix(body->parent) * localTransform;
		}

		return localTransform;
	};

	void Renderer::setMaterialUniforms(Material* material, Program* program, int materialId) {
		for (auto iterator = material->materialParameters.begin(); iterator != material->materialParameters.end(); ++iterator) {
			string paramName = (*iterator).first;
			optional<ParamData> paramData = material->getParameter(paramName);
			bool boolData = false;
			int intData = 0;
			float floatData = 0.0f;
			Vector2f v2Data = { 0,0 };
			Vector3f v3Data = { 0,0,0 };
			Color colorData = { 0,0,0,0 };
			Texture* textureData = nullptr;
			if (paramData.has_value()) {
				any paramVal = paramData.value().data;
				ParamType paramType = paramData.value().type;
				switch (paramType) {
				case ParamType::Bool:
					boolData = any_cast<bool>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), boolData);
					break;
				case ParamType::Int:
					intData = any_cast<int>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), intData);
					break;
				case ParamType::Float:
					floatData = any_cast<float>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), floatData);
					break;
				case ParamType::V2:
					v2Data = any_cast<Vector2f>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), toGlm(v2Data));
					break;
				case ParamType::V3:
					v3Data = any_cast<Vector3f>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), toGlm(v3Data));
					break;
				case ParamType::RGBA:
					colorData = any_cast<Color>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), toGlm(colorData));
					break;
				case ParamType::Texture2D:
					textureData = any_cast<Texture*>(paramVal);
					if (textureData != nullptr) {
						glActiveTexture(GL_TEXTURE0 + boundTextures);
						glBindTexture(GL_TEXTURE_2D, textureData->getNativeHandle());
						program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), boundTextures);
						boundTextures++;
					}
					break;
				//case ParamType::String:
					//	break;
				default:
					break;
				}
			}
		}
	}

	void Renderer::setLightUniforms(Light* light, size_t lightIndex, Program* program) {
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "position").c_str(), light->position);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "brightness").c_str(), light->parameters.brightness);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "intensities").c_str(), toGlm(light->parameters.colorIntensities));
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "attenuation").c_str(), light->parameters.attenuation);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "ambiance").c_str(), light->parameters.ambiance);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "coneAngle").c_str(), light->parameters.coneAngle);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "lightDirection").c_str(), toGlm(light->parameters.lightDirection));
	}

	string Renderer::getUniformArrayIndexName(string arrayName, int index) {
		std::ostringstream ss;
		ss << arrayName << "[" << index << "]";
		return ss.str();
	}

	string Renderer::getUniformArrayPropertyName(string arrayName, int index, string propertyName) {
		std::ostringstream ss;
		ss << arrayName << "[" << index << "]." << propertyName;
		return ss.str();
	}

	string Renderer::getUniformObjectPropertyName(string objectName, string propertyName) {
		std::ostringstream ss;
		ss << objectName << "." << propertyName;
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

	glm::vec2 Renderer::toGlm(Vector2f v) {
		return glm::vec2(v.x, v.y);
	}

	glm::vec3 Renderer::toGlm(Vector3f v) {
		return glm::vec3(v.x, v.y, v.z);
	}

	glm::vec3 Renderer::toGlm(Color c) {
		return glm::vec3(c.r, c.g, c.b);
	}

	Vector2f Renderer::fromGlm(glm::vec2 v) {
		return Vector2f(v.x, v.y);
	}

	Vector3f Renderer::fromGlm(glm::vec3 v) {
		return Vector3f(v.x, v.y, v.z);
	}

	const aiScene* Renderer::readFile(string path, unsigned int options) {
		return importer->readFile(path, options);
	}
}