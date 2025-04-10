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
		GLenum errorCode = GL_NO_ERROR;
#ifdef DEBUG
		while ((errorCode = glGetError()) != GL_NO_ERROR) {
			string error;
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
#endif
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
		currentCamera = make_unique<Camera>(aspectRatio);

		// Enable Z-buffer read and write
		GL_CHECK(glEnable(GL_DEPTH_TEST));
		GL_CHECK(glEnable(GL_BLEND));
		GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

		// Initialize UBOs
		glGenBuffers(1, &materialUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, materialUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(MaterialUBO), nullptr, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 3, materialUBO);

		glGenBuffers(1, &lightUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, lightUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(LightUBO), nullptr, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 4, lightUBO);

		glGenBuffers(1, &boneUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, boneUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(BoneUBO), nullptr, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, boneUBO);

		glGenBuffers(1, &transformUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, transformUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(TransformUBO), nullptr, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 2, transformUBO);

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

			vector<id_t> meshMaterialIds = mesh->getMaterials();
			Material* renderMaterial = assets.get<Material>(fallbackMaterialId);
			if (meshMaterialIds.size() > 0) {
				Material* meshMaterial = assets.get<Material>(meshMaterialIds[0]);
				if (meshMaterial && meshMaterial->getProgram()) {
					renderMaterial = meshMaterial;
				}
			}

			Program* program = renderMaterial->getProgram();
			if (!program) {
				log(this, LogError, "Shader program is invalid in getModelData");
				return;
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

			setGLWindowState(false);
		}
	}

	void Renderer::updateMaterialUBO(const MaterialUBO& materialData) {
		glBindBuffer(GL_UNIFORM_BUFFER, materialUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MaterialUBO), &materialData);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void Renderer::updateLightUBO(const LightUBO& lightData) {
		glBindBuffer(GL_UNIFORM_BUFFER, lightUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightUBO), &lightData);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void Renderer::updateBoneUBO(const BoneUBO& boneData) {
		glBindBuffer(GL_UNIFORM_BUFFER, boneUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(BoneUBO), &boneData);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void Renderer::updateTransformUBO(const TransformUBO& transformData) {
		glBindBuffer(GL_UNIFORM_BUFFER, transformUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(TransformUBO), &transformData);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	ImportResult Renderer::import(string path, const string& skeletonName) {
		return importer->importModel(path,skeletonName);
	}

	Material* Renderer::getFallbackMaterial() {
		return assets.get<Material>(fallbackMaterialId);
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
			// Activate OpenGL context once for the entire render pass
			if (!setGLWindowState(true)) return false;
			//Clear render order
			clear();
			//Collect Bodies to render
			Body* root = world->getRoot();
			if (!root) {
				log(this, LogError, "Root is null");
				setGLWindowState(false);
				return false;
			}
			root->render(*window, root->getTransform());
			//Sort and render Meshes and SFML entities
			render(window);
			endFrame();

#ifdef DEBUG
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
#endif

			window->display();
			setGLWindowState(false);
			return true;
		} catch (const exception& ex) {
			log(this, LogError, "Exception in processRender: {}", ex.what());
			setGLWindowState(false);
			return false;
		} catch (...) {
			log(this, LogError, "Unknown exception in processRender");
			setGLWindowState(false);
			return false;
		}
	}

	void Renderer::renderMesh(Mesh* mesh, MeshData* meshData, Transformation3D transform) {
		if (!meshData || meshData->vertices.empty()) return;	//Don;t log, null MeshData Meshes are empty Bodies
		if (!mesh) {
			log(this, LogError, "Mesh is invalid");
			return;
		}
		if (!mesh->getBodyId().has_value()) {
			log(this, LogError, "Mesh Body ID is empty in renderMesh");
			return;
		}

		// Only proceed with mesh rendering if we have mesh data
		if (meshData && meshData->vertices.size() > 0) {
			GL_CHECK(glBindVertexArray(meshData->vao));
			boundTextures = 0;
			
			// Get materials Mesh, ensuring at least one material is present
			vector<id_t> modelMaterials = mesh->getMaterials();
			if (modelMaterials.empty()) {
				log(this, LogWarn, "No model materials in renderer. Using fallback.");
				modelMaterials.push_back(assets.getDefaultId<Material>().value());
			}
			Material* renderMaterial = assets.get<Material>(modelMaterials.at(0));

			//Update each model's animator, if present, once per frame (instead of once per mesh per model per frame)
			Animator* animator = updateAnimator(mesh);

			//Get the renderMaterial's program and bind it
			Program* program = useRenderProgram(renderMaterial);
			if (!program) return;

			//Set the standard material uniform values
			MaterialUBO materialUBOData = MaterialUBO();
			setMaterialUBOData(materialUBOData, modelMaterials, program);
			LightUBO lightUBOData = LightUBO();
			setLightUBOData(lightUBOData);
			TransformUBO transformUBOData = TransformUBO();
			setTransformUBOData(transformUBOData, getBodyGlobalTransform(mesh->getBodyId()));
			//If the mesh has bones, set the bone uniform values
			if (animator) {
				BoneUBO boneUBOData = BoneUBO();
				setBoneUBOData(boneUBOData, animator);
			}
			program->setUniform("cameraPosition", toGlm(currentCamera->getPosition()));
			program->setUniform("timeSec", time.getElapsedSec());

			//Draw the MeshData by index, if available, or by vertices
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

	void Renderer::endFrame() {
		updatedModels.clear();
	}

	glm::mat4 Renderer::getBodyGlobalTransform(optional<id_t> bodyId) {
		//Get the Mesh Body's transformation matrix
		if (!bodyId.has_value()) return glm::mat4(1.0);
		Body* meshBody = assets.get<Body>(bodyId.value());
		if (!meshBody) {
			log(this, LogError, "Mesh Body is null in renderMesh");
			return glm::mat4(1.0);
		}
		return getCombinedModelMatrix(meshBody);
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

	Animator* Renderer::updateAnimator(Mesh* mesh) {
		//Update each model's animator, if present, once per frame (instead of once per mesh per model per frame)
		Animator* animator = nullptr;
		optional<id_t> meshModelId = mesh->getModelId();
		if (meshModelId.has_value()) {
			Model* meshModel = assets.get<Model>(meshModelId.value());
			// Only update animation once per model per frame
			if (meshModel && updatedModels.find(meshModelId.value()) == updatedModels.end()) {
				animator = meshModel->getAnimator();
				if (animator) {
					animator->updateAnimation(time.getDeltaSec());
					updatedModels.insert(meshModelId.value());
				}
			}
		}
		return animator;
	}

	Program* Renderer::useRenderProgram(Material* renderMaterial) {
		Program* program = renderMaterial->getProgram();
		if (!program) {
			log(this, LogError, "Shader program is invalid");
			return nullptr;
		}
		program->use();
		return program;
	}

	void Renderer::bindTextureAndSetUniform(Material* material, const string& paramName, Program* program, int materialIndex, int& boundTextures) {
		optional<ParamData> paramData = material->getParameter(paramName);
		if (paramData.has_value()) {
			Texture* textureData = any_cast<Texture*>(paramData.value().data);
			if (textureData != nullptr) {
				GL_CHECK(glActiveTexture(GL_TEXTURE0 + boundTextures));
				GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureData->getNativeHandle()));
				program->setUniform(getUniformArrayPropertyName("materialTextures", materialIndex, paramName).c_str(), boundTextures);
				boundTextures++;
			}
		}
	}

	void Renderer::setMaterialUBOData(MaterialUBO materialUBOData, vector<id_t> modelMaterials, Program* program) {
		static MaterialUBO previousMaterialUBOData;
		bool materialChanged = false;
		
		for (int i = 0; i < modelMaterials.size(); ++i) {
			//Get the material
			Material* material = assets.get<Material>(modelMaterials.at(i));
			if (!material) continue;
			optional<ParamData> paramData = nullopt;
			//For diffuse, specular and opacity textures: Get the texture data, if available, and bind it to the shader, then set the materialTextures[i].diff/spec/opacityTexture uniform
			bindTextureAndSetUniform(material, "diffuseTexture", program, i, boundTextures);
			bindTextureAndSetUniform(material, "specularTexture", program, i, boundTextures);
			bindTextureAndSetUniform(material, "opacityTexture", program, i, boundTextures);

			paramData = material->getParameter("diffuseColor");
			if (paramData.has_value()) materialUBOData.materials[i].diffuseColor = glm::vec4(toGlm(any_cast<Color>(paramData.value().data)), 1);
			paramData = material->getParameter("diffuseTextureUVScale");
			if (paramData.has_value()) materialUBOData.materials[i].diffuseTextureUVScale = toGlm(any_cast<Vector2f>(paramData.value().data));
			paramData = material->getParameter("diffuseTextureOffset");
			if (paramData.has_value()) materialUBOData.materials[i].diffuseTextureOffset = toGlm(any_cast<Vector2f>(paramData.value().data));
			paramData = material->getParameter("diffuseTextureScrollSpeed");
			if (paramData.has_value()) materialUBOData.materials[i].diffuseTextureScrollSpeed = toGlm(any_cast<Vector2f>(paramData.value().data));
			paramData = material->getParameter("specularColor");
			if (paramData.has_value()) materialUBOData.materials[i].specularColor = glm::vec4(toGlm(any_cast<Color>(paramData.value().data)), 1);
			paramData = material->getParameter("specularTextureUVScale");
			if (paramData.has_value()) materialUBOData.materials[i].specularTextureUVScale = toGlm(any_cast<Vector2f>(paramData.value().data));
			paramData = material->getParameter("specularTextureOffset");
			if (paramData.has_value()) materialUBOData.materials[i].specularTextureOffset = toGlm(any_cast<Vector2f>(paramData.value().data));
			paramData = material->getParameter("specularTextureScrollSpeed");
			if (paramData.has_value()) materialUBOData.materials[i].specularTextureScrollSpeed = toGlm(any_cast<Vector2f>(paramData.value().data));
			paramData = material->getParameter("opacityTextureUVScale");
			if (paramData.has_value()) materialUBOData.materials[i].opacityTextureUVScale = toGlm(any_cast<Vector2f>(paramData.value().data));
			paramData = material->getParameter("opacityTextureOffset");
			if (paramData.has_value()) materialUBOData.materials[i].opacityTextureOffset = toGlm(any_cast<Vector2f>(paramData.value().data));
			paramData = material->getParameter("opacityTextureScrollSpeed");
			if (paramData.has_value()) materialUBOData.materials[i].opacityTextureScrollSpeed = toGlm(any_cast<Vector2f>(paramData.value().data));
			paramData = material->getParameter("smoothnessFactor");
			if (paramData.has_value()) materialUBOData.materials[i].smoothnessFactor = any_cast<float>(paramData.value().data);
			paramData = material->getParameter("opacity");
			if (paramData.has_value()) materialUBOData.materials[i].opacity = any_cast<float>(paramData.value().data);
			paramData = material->getParameter("alphaCutoff");
			if (paramData.has_value()) materialUBOData.materials[i].alphaCutoff = any_cast<float>(paramData.value().data);
			paramData = material->getParameter("gamma");
			if (paramData.has_value()) materialUBOData.materials[i].gamma = any_cast<float>(paramData.value().data);
			paramData = material->getParameter("opacityMasked");
			if (paramData.has_value()) materialUBOData.materials[i].opacityMasked = any_cast<bool>(paramData.value().data);
			paramData = material->getParameter("useGammaCorrection");
			if (paramData.has_value()) materialUBOData.materials[i].useGammaCorrection = any_cast<bool>(paramData.value().data);
			paramData = material->getParameter("useDiffuseTexture");
			if (paramData.has_value()) materialUBOData.materials[i].useDiffuseTexture = any_cast<bool>(paramData.value().data);
			paramData = material->getParameter("useSpecularTexture");
			if (paramData.has_value()) materialUBOData.materials[i].useSpecularTexture = any_cast<bool>(paramData.value().data);
			paramData = material->getParameter("useOpacityTexture");
			if (paramData.has_value()) materialUBOData.materials[i].useOpacityTexture = any_cast<bool>(paramData.value().data);
			paramData = material->getParameter("useLighting");
			if (paramData.has_value()) materialUBOData.materials[i].useLighting = any_cast<bool>(paramData.value().data);
		}
		//TODO: Add support for dynamic materials
		// Check if material UBO data has changed
		if (memcmp(&materialUBOData, &previousMaterialUBOData, sizeof(MaterialUBO)) != 0) {
			materialChanged = true;
			previousMaterialUBOData = materialUBOData;
		}

		//Only update the UBO if the data has changed
		if (materialChanged) {
			updateMaterialUBO(materialUBOData);
		}
	}

	void Renderer::setLightUBOData(LightUBO lightUBOData) {
		static LightUBO previousLightUBOData;
		bool lightChanged = false;

		lightUBOData.lightCount = assets.getResourceCount<Light>();
		for (size_t i = 0; i < assets.getResourceCount<Light>(); ++i) {
			Light* light = assets.get<Light>(i);
			lightUBOData.lights[i].position = light->position;
			lightUBOData.lights[i].brightness = light->parameters.brightness;
			lightUBOData.lights[i].intensities = glm::vec4(toGlm(light->parameters.colorIntensities), 1);
			lightUBOData.lights[i].attenuation = light->parameters.attenuation;
			lightUBOData.lights[i].ambiance = light->parameters.ambiance;
			lightUBOData.lights[i].coneAngle = light->parameters.coneAngle;
			lightUBOData.lights[i].lightDirection = glm::vec4(toGlm(light->parameters.lightDirection), 1);
		}
		//TODO: Add support for dynamic lights
		// Check if light UBO data has changed
		if (memcmp(&lightUBOData, &previousLightUBOData, sizeof(LightUBO)) != 0) {
			lightChanged = true;
			previousLightUBOData = lightUBOData;
		}

		//Only update the UBO if the data has changed
		if (lightChanged) {
			updateLightUBO(lightUBOData);
		}
	}

	void Renderer::setBoneUBOData(BoneUBO boneUBOData, Animator* animator) {
		vector<glm::mat4> transforms = animator->getBoneMatrices();
		for (int i = 0; i < transforms.size(); ++i) {
			boneUBOData.boneMatrices[i] = transforms[i];
		}
		boneUBOData.boneCount = transforms.size();
		updateBoneUBO(boneUBOData);
	}

	void Renderer::setTransformUBOData(TransformUBO transformUBOData, glm::mat4 combinedTransform) {
		transformUBOData.model = combinedTransform;
		transformUBOData.camera = currentCamera->getMatrix();
		updateTransformUBO(transformUBOData);
	}

	void Renderer::setMaterialUniforms(id_t materialAssetId, Program* program, int materialId) {
		Material* material = assets.get<Material>(materialAssetId);
		if (!material) return;
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

	void Renderer::add(id_t bodyId, Transform transform) {
		renderOrder.push_back(bodyId);
	}

	int Renderer::zMax() {
		Body* body = assets.get<Body>(renderOrder.back());
		return body->zOrder;
	}

	int Renderer::zMin() {
		Body* body = assets.get<Body>(renderOrder.back());
		return body->zOrder;
	}

	vector<id_t> Renderer::getZBodies(int zIndex) {
		vector<id_t> bodies;
		bool found = false;
		for (int i = 0; i < renderOrder.size(); ++i) {
			Body* body = assets.get<Body>(renderOrder.at(i));
			if (body->zOrder == zIndex) {
				found = true;
				bodies.push_back(renderOrder.at(i));
			}
			else if (found) {
				break;
			}
		}
		return bodies;
	}

	vector<id_t> Renderer::getLowerZBodies(int zIndex) {
		vector<id_t> bodies;
		bool found = false;
		for (int i = 0; i < renderOrder.size(); ++i) {
			Body* body = assets.get<Body>(renderOrder.at(i));
			if (body->zOrder < zIndex) {
				bodies.push_back(renderOrder.at(i));
			}
			else {
				break;
			}
		}
		return bodies;
	}

	vector<id_t> Renderer::getHigherZBodies(int zIndex) {
		vector<id_t> bodies;
		bool found = false;
		for (int i = renderOrder.size() - 1; i >= 0; --i) {
			Body* body = assets.get<Body>(renderOrder.at(i));
			if (body->zOrder > zIndex) {
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
		sort(renderOrder.begin(), renderOrder.end(), [](id_t a, id_t b) { return (assets.get<Body>(a)->zOrder < assets.get<Body>(b)->zOrder); });
	}

	void Renderer::render(RenderTarget* window) {
		//Sort the renderOrder by zOrder if zSorting is enabled
		if (zSortingEnabled) {
			sortZ();
		}

		//Draw each body with its calculated transform
		for (auto iterator = renderOrder.begin(); iterator != renderOrder.end(); ++iterator) {
			Body* body = assets.get<Body>(*iterator);
			body->onDraw(*window, body->getGlobalTransform());
		}
	}

	Camera* Renderer::getCurrentCamera() {
		return currentCamera.get();
	}

	void Renderer::setCurrentCamera(unique_ptr<Camera> camera) {
		currentCamera = move(camera);
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