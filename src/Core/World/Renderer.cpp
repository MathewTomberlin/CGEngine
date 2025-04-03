#include "../Engine/Engine.h"
#include "Renderer.h"
#include "../../Standard/Models/CommonModels.h"
#include "../Animation/Animator.h"
#include "../Mesh/Model.h"

namespace CGEngine {
	void Renderer::setWindow(RenderWindow* window) {
		this->window = window;
	}

	GLenum Renderer::checkGLError(const char* operation, const char* file, int line) {
		GLenum errorCode;
		while ((errorCode = glGetError()) != GL_NO_ERROR) {
			std::string error;
			switch (errorCode) {
			case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
			case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
			case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
			case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
			case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
			case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
			case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
			default:                               error = "Unknown Error"; break;
			}
			log(this, LogError, "OpenGL Error: {} [{}] ({}) at {}:{}", error, (int)errorCode, operation, file, line);
		}
		return errorCode;
	}

	bool Renderer::validateShader(Shader* shader) {
		if (!shader || !shader->isValid()) {
			log(this, LogError, "Invalid shader");
			return false;
		}
		return true;
	}

	bool Renderer::validateProgram(Program* program) {
		if (!program || !program->isValid()) {
			log(this, LogError, "Invalid shader program");
			return false;
		}

		GLuint programId = program->getObjectId();
		GLint status;
		GL_CHECK(glValidateProgram(programId));
		GL_CHECK(glGetProgramiv(programId, GL_VALIDATE_STATUS, &status));

		if (status == GL_FALSE) {
			GLint infoLength;
			GL_CHECK(glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLength));

			std::vector<char> infoLog(infoLength + 1);
			GL_CHECK(glGetProgramInfoLog(programId, infoLength, NULL, infoLog.data()));

			log(this, LogError, "Program validation failed: {}", infoLog.data());
			return false;
		}

		return true;
	}

	void Renderer::initializeOpenGL() {
		if (!setGLWindowState(true)) return;

		// Setup a camera with perspective projection
		GLfloat aspectRatio = static_cast<float>(window->getSize().x) / window->getSize().y;
		currentCamera = new Camera(aspectRatio);

		// Enable Z-buffer read and write
		GL_CHECK(glEnable(GL_DEPTH_TEST));
		GL_CHECK(glEnable(GL_BLEND));
		GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
		// Check for any OpenGL errors that may have occurred during initialization
		checkGLError("initializeOpenGL", __FILE__, __LINE__);

		commitGL();
		if (!setGLWindowState(false)) return;
	}

	void Renderer::getModelData(Mesh* mesh) {
		if (setGLWindowState(true)) {
			MeshData* meshData = mesh->getMeshData();
			//Don't throw an error because null Mesh Bodies are valid (but not rendered)
			if (!meshData) return;

			vector<Material*> material = mesh->getMaterials();
			Material* renderMaterial = assets.get<Material>(fallbackMaterialId); //TODO: Ensure Renderer.fallbackMaterial is using AssetManager
			if (material.size() > 0 && material[0] && material[0]->getProgram()) {
				renderMaterial = material[0];
			}

			Program* program = renderMaterial->getProgram();
			if (!program) {
				log(this, LogError, "Shader program is invalid in getModelData");
			}

			GL_CHECK(glGenBuffers(1, & meshData->vbo));
			GL_CHECK(glGenBuffers(1, &meshData->ebo));
			GL_CHECK(glGenVertexArrays(1, &meshData->vao));

			GL_CHECK(glBindVertexArray(meshData->vao));
			//Bind the vertex buffer and pass in the vertex data
			GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER,  meshData->vbo);
			GL_CHECK(glBufferData(GL_ARRAY_BUFFER,  meshData->vertices.size() 
				* sizeof(VertexData), meshData->vertices.data(), GL_STATIC_DRAW)));

			//Bind and buffert the element buffer, if the model has indices
			if (meshData->indices.size() > 0) {
				GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,  meshData->ebo));
				GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER,  meshData->indices.size() 
					* sizeof(unsigned int), meshData->indices.data(), GL_STATIC_DRAW));
			}

			//Enable and prepare vertex, texture coordinate, normal, and textureId attribute arrays
			GL_CHECK(glEnableVertexAttribArray(program->attrib("position")));
			GL_CHECK(glVertexAttribPointer(program->attrib("position"), 3, GL_FLOAT, 
				GL_FALSE, sizeof(VertexData), NULL));

			GL_CHECK(glEnableVertexAttribArray(program->attrib("texCoord")));
			GL_CHECK(glVertexAttribPointer(program->attrib("texCoord"), 2, GL_FLOAT, 
				GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData,texCoord)));

			GL_CHECK(glEnableVertexAttribArray(program->attrib("vertNormal")));
			GL_CHECK(glVertexAttribPointer(program->attrib("vertNormal"), 3, GL_FLOAT, 
				GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal)));

			GL_CHECK(glEnableVertexAttribArray(program->attrib("materialId")));
			GL_CHECK(glVertexAttribPointer(program->attrib("materialId"), 1, GL_FLOAT, 
				GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, materialId)));

			if (!meshData->bones.empty()) {
				GL_CHECK(glEnableVertexAttribArray(program->attrib("boneIds")));
				GL_CHECK(glVertexAttribIPointer(program->attrib("boneIds"), 4, GL_INT, 
					sizeof(VertexData), (void*)offsetof(VertexData, boneIds)));

				GL_CHECK(glEnableVertexAttribArray(program->attrib("weights")));
				GL_CHECK(glVertexAttribPointer(program->attrib("weights"), 4, GL_FLOAT, 
					GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, weights)));
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
		return assets.get<Material>(fallbackMaterialId);
	}

	void Renderer::updateModelData(Mesh* mesh) {
		if (setGLWindowState(true)) {
			MeshData* meshData = mesh->getMeshData();
			//Don't throw an error because null Mesh Bodies are valid (but not rendered)
			if (!meshData) return;

			vector<Material*> material = mesh->getMaterials();
			Material* renderMaterial = assets.get<Material>(fallbackMaterialId); //TODO: Ensure Renderer.fallbackMaterial is using AssetManager
			if (material.size() > 0 && material[0] && material[0]->getProgram()) {
				renderMaterial = material[0];
			}

			GL_CHECK(glBindVertexArray(meshData->vao));

			//Bind the vertex buffer and pass in the vertex data
			GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
			GL_CHECK(glBufferData(GL_ARRAY_BUFFER, meshData->vertices.size() 
				* sizeof(VertexData), meshData->vertices.data(), GL_STATIC_DRAW));

			//Bind and buffert the element buffer, if the model has indices
			if (meshData->indices.size() > 0) {
				GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData->ebo));
				GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData->indices.size() 
					* sizeof(unsigned int), meshData->indices.data(), GL_STATIC_DRAW));
			}
			glBindVertexArray(0);

			mesh->setMeshData(meshData);
			setGLWindowState(false);
		}
	}

	bool Renderer::setGLWindowState(bool state) {
		// Make the window no longer the active window for OpenGL calls
		bool success = window->setActive(state);
		if (!success) {
			log(this, LogError, "Failed to set window state to {}", (state) ? "active" : "inactive");
		}
		return success;
	}

	bool Renderer::clearGL(GLbitfield mask) {
		if (!setGLWindowState(true)) return false;
		GL_CHECK(glClear(mask));
		return true;
	}

	void Renderer::commitGL() {
		window->pushGLStates();
	}

	void Renderer::pullGL() {
		window->popGLStates();
	}

	bool Renderer::processRender() {
		try {
			//Clear render order
			clear();
			//Collect Bodies to render
			Body* root = world->getRoot();
			if (!root) {
				log(this, LogError, "Root is null");
				return false;
			}
			root->render(*window, root->getTransform());
			//Sort and render Meshes and SFML entities
			render(window);
			endFrame();

			// Check for errors before display
			GLenum err = glGetError();
			if (err != GL_NO_ERROR) {
				log(this, LogWarn, "OpenGL errors detected before display, attempting recovery");
				// Recovery: clear any bound buffers or textures
				glBindVertexArray(0);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
				glUseProgram(0);
				// Reset texture units
				for (int i = 0; i < 8; i++) {
					glActiveTexture(GL_TEXTURE0 + i);
					glBindTexture(GL_TEXTURE_2D, 0);
				}
			}

			window->display();
			return true;
		} catch (const exception& ex) {
			log(this, LogError, "Exception in processRender: {}", ex.what());
			return false;
		} catch (...) {
			log(this, LogError, "Unknown exception in processRender");
			return false;
		}
	}

	void Renderer::renderMesh(Mesh* mesh, MeshData* meshData, Transformation3D transform) {
		if (!meshData || meshData->vertices.empty()) return;
		if (!mesh) {
			log(this, LogError, "Mesh is invalid");
			return;
		}
		
		Body* meshBody = mesh->getBody();
		if (!meshBody) {
			log(this, LogError, "Mesh body is null in rendermesh");
			return;
		}
		// Get combined transform from entire hierarchy
		glm::mat4 combinedTransform = getCombinedModelMatrix(meshBody);

		Model* meshModel = mesh->getModel();
		if (renderer.setGLWindowState(true)) {
			// Only proceed with mesh rendering if we have mesh data
			if (meshData && meshData->vertices.size() > 0) {
				GL_CHECK(glBindVertexArray(meshData->vao));
				boundTextures = 0;
				
				// Get materials from Model instead of Mesh
				vector<Material*> modelMaterials = mesh->getMaterials();
				// Ensure at least one material (fallback)
				if (modelMaterials.empty()) {
					log(this, LogWarn, "No model materials in renderer. Using fallback.");
					modelMaterials.push_back(assets.get<Material>(fallbackMaterialId)); //TODO: Ensure Renderer.fallbackMaterial is using AssetManager
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
				if (!program) {
					log(this, LogError, "Shader program is invalid");
					return;
				}
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

				program->setUniform("lightCount", (int)assets.getResourceCount<Light>());
				for (size_t i = 0; i < assets.getResourceCount<Light>(); ++i) {
					setLightUniforms(assets.get<Light>(i), i, program);
				}

				// Draw the cube
				if (meshData->indices.size()) {
					GL_CHECK(glDrawElements(GL_TRIANGLES, meshData->indices.size(), GL_UNSIGNED_INT, 0));
				} else {
					GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, meshData->vertices.size() / 5));
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

			//Data buffers
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
					try {
						boolData = any_cast<bool>(paramVal);
						program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), boolData);
					} catch (bad_any_cast ex) {
						log(this, LogWarn, "Failed to set bool shader uniform {} from material {}: {}", paramName, materialId, ex.what());
					}
					break;
				case ParamType::Int:
					try {
						intData = any_cast<int>(paramVal);
						program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), intData);
					} catch (bad_any_cast ex) {
						log(this, LogWarn, "Failed to set int shader uniform {} from material {}: {}", paramName, materialId, ex.what());
					}
					break;
				case ParamType::Float:
					try {
						floatData = any_cast<float>(paramVal);
						program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), floatData);
					} catch (bad_any_cast ex) {
						log(this, LogWarn, "Failed to set float shader uniform {} from material {}: {}", paramName, materialId, ex.what());
					}
					break;
				case ParamType::V2:
					try {
						v2Data = any_cast<Vector2f>(paramVal);
						program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), toGlm(v2Data));
					} catch (bad_any_cast ex) {
						log(this, LogWarn, "Failed to set v2 shader uniform {} from material {}: {}", paramName, materialId, ex.what());
					}
					break;
				case ParamType::V3:
					try {
						v3Data = any_cast<Vector3f>(paramVal);
						program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), toGlm(v3Data));
					} catch (bad_any_cast ex) {
						log(this, LogWarn, "Failed to set v3 shader uniform {} from material {}: {}", paramName, materialId, ex.what());
					}
					break;
				case ParamType::RGBA:
					try {
						colorData = any_cast<Color>(paramVal);
						program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), toGlm(colorData));
					} catch (bad_any_cast ex) {
						log(this, LogWarn, "Failed to set color shader uniform {} from material {}: {}", paramName, materialId, ex.what());
					}
					break;
				case ParamType::Texture2D:
					try{
						textureData = any_cast<Texture*>(paramVal);
						if (textureData != nullptr) {
							GL_CHECK(glActiveTexture(GL_TEXTURE0 + boundTextures));
							GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureData->getNativeHandle()));
							program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), boundTextures);
							boundTextures++;
						}
					} catch (bad_any_cast ex) {
						log(this, LogWarn, "Failed to set texture2D shader uniform {} from material {}: {}", paramName, materialId, ex.what());
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
			log(this, LogError, "GLEW initialization failed: {}", (const char*)glewGetErrorString(state));
			return state;
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