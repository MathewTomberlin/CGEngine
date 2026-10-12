#include <algorithm>
#include "../Engine/Engine.h"
#include "Renderer.h"
#include "../../Standard/Models/CommonModels.h"
#include "../Animation/Animator.h"
#include "../Mesh/Model.h"

#include "../Body/Body.h"
#include "../Shader/Shader.h"
#include "../Shader/Program.h"
#include "../Light/Light.h"
#include "../Material/Material.h"

namespace CGEngine {
	Renderer::Renderer() {
		init();
		importer = new MeshImporter();
	}

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
		// Start with identity bones: undefined contents here would skin meshes into garbage before the first pose is sent.
		BoneUBO identityBones;
		for (int i = 0; i < BoneUBO::MAX_BONES; ++i) identityBones.boneMatrices[i] = glm::mat4(1.0f);
		identityBones.boneCount = 0;
		glBufferData(GL_UNIFORM_BUFFER, sizeof(BoneUBO), &identityBones, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, boneUBO);

		glGenBuffers(1, &transformUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, transformUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(TransformUBO), nullptr, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 2, transformUBO);

		skyProgram = make_unique<Program>(ShaderProgramPath("shaders/Sky.vert", "shaders/Sky.frag"), "sky");
		glGenVertexArrays(1, &skyVao);

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
			// Upload once per MeshData. Each Mesh over the same data shares its buffers and VAO.
			if (meshData->vao != 0U) return;

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

			if (meshData->compact) {
				createCompactMeshBuffers(meshData, program);
				meshData->releaseCpuCopy();
				setGLWindowState(false);
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

			GL_CHECK(glEnableVertexAttribArray(program->attrib("materialId")));
			GL_CHECK(glVertexAttribPointer(program->attrib("materialId"), 1, GL_FLOAT,
				GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, materialId)));

			// Vertex colour. Shaders without the attribute get -1 and skip it.
			if (program->attrib("vertColour") >= 0) {
				GL_CHECK(glEnableVertexAttribArray(program->attrib("vertColour")));
				GL_CHECK(glVertexAttribPointer(program->attrib("vertColour"), 3, GL_FLOAT,
					GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, colour)));
			}

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

	void Renderer::uploadMeshData(MeshData* meshData) {
		// Buffers already exist (getModelData made them): replace their contents. The VAO keeps its attribute setup.
		meshData->gpuDirty = false;
		if (meshData->vao == 0U) return;
		GL_CHECK(glBindVertexArray(meshData->vao));
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
		if (meshData->compact) {
			GL_CHECK(glBufferData(GL_ARRAY_BUFFER, meshData->voxelVertices.size() * sizeof(VoxelVertex), meshData->voxelVertices.data(), GL_DYNAMIC_DRAW));
		} else {
			GL_CHECK(glBufferData(GL_ARRAY_BUFFER, meshData->vertices.size() * sizeof(VertexData), meshData->vertices.data(), GL_DYNAMIC_DRAW));
		}
		if (!meshData->indices.empty()) {
			// Binding the element buffer while the VAO is bound makes it part of the VAO, even if it had no indices before.
			GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData->ebo));
			GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData->indices.size() * sizeof(unsigned int), meshData->indices.data(), GL_DYNAMIC_DRAW));
		}
		GL_CHECK(glBindVertexArray(0));
		meshData->releaseCpuCopy();
	}

	void Renderer::createCompactMeshBuffers(MeshData* meshData, Program* program) {
		GL_CHECK(glGenBuffers(1, &meshData->vbo));
		GL_CHECK(glGenBuffers(1, &meshData->ebo));
		GL_CHECK(glGenVertexArrays(1, &meshData->vao));
		GL_CHECK(glBindVertexArray(meshData->vao));
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
		GL_CHECK(glBufferData(GL_ARRAY_BUFFER, meshData->voxelVertices.size() * sizeof(VoxelVertex), meshData->voxelVertices.data(), GL_STATIC_DRAW));
		if (!meshData->indices.empty()) {
			GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData->ebo));
			GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData->indices.size() * sizeof(unsigned int), meshData->indices.data(), GL_STATIC_DRAW));
		}
		// Every voxel attribute is a float: byte attributes, even normalised, made each draw several times slower on
		// the NVIDIA driver (measured in the voxel sample).
		auto bind = [&](const char* name, GLint size, GLenum type, GLboolean normalized, size_t offset) {
			const GLint location = program->attrib(name);
			if (location < 0) return;
			GL_CHECK(glEnableVertexAttribArray(location));
			GL_CHECK(glVertexAttribPointer(location, size, type, normalized, sizeof(VoxelVertex), (void*)offset));
		};
		bind("position", 3, GL_FLOAT, GL_FALSE, offsetof(VoxelVertex, position));
		bind("texCoord", 2, GL_FLOAT, GL_FALSE, offsetof(VoxelVertex, texCoord));
		bind("vertNormal", 3, GL_FLOAT, GL_FALSE, offsetof(VoxelVertex, normal));
		bind("vertGlow", 3, GL_FLOAT, GL_FALSE, offsetof(VoxelVertex, glow));
		bind("vertSky", 1, GL_FLOAT, GL_FALSE, offsetof(VoxelVertex, sky));
		bind("vertDamage", 1, GL_FLOAT, GL_FALSE, offsetof(VoxelVertex, damage));
		bind("vertFlicker", 1, GL_FLOAT, GL_FALSE, offsetof(VoxelVertex, flicker));
		bind("materialId", 1, GL_FLOAT, GL_FALSE, offsetof(VoxelVertex, materialId));
		bind("vertColour", 3, GL_FLOAT, GL_FALSE, offsetof(VoxelVertex, colour));
		GL_CHECK(glBindVertexArray(0));
	}

	void Renderer::releaseMeshData(MeshData* meshData) {
		if (setGLWindowState(true)) {
			if (meshData->vao != 0U) glDeleteVertexArrays(1, &meshData->vao);
			if (meshData->vbo != 0U) glDeleteBuffers(1, &meshData->vbo);
			if (meshData->ebo != 0U) glDeleteBuffers(1, &meshData->ebo);
			meshData->vao = meshData->vbo = meshData->ebo = 0U;
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
		// setActive switches the GL context (wglMakeCurrent) and costs ~0.7 ms, so skip redundant calls.
		if (state == glWindowActive) return true;
		bool success = window->setActive(state);
		if (!success) {
			log(this, LogError, "Failed to set window state to {}", (state) ? "active" : "inactive");
		} else {
			glWindowActive = state;
		}
		return success;
	}

	bool Renderer::clearGL(GLbitfield mask) {
		GL_CHECK(glClearColor(environment.skyColour.x, environment.skyColour.y, environment.skyColour.z, 1.0f));
		GL_CHECK(glClear(mask));
		return true;
	}

	void Renderer::commitGL() {
		// Each push and pop saves or restores every GL state, so only switch when the mode changes.
		if (sfmlGLStatesActive) return;
		window->pushGLStates();
		sfmlGLStatesActive = true;
	}

	void Renderer::pullGL() {
		if (!sfmlGLStatesActive) return;
		window->popGLStates();
		sfmlGLStatesActive = false;
	}

	void Renderer::saveScreenshot(const std::string& path) {
		Vector2u size = window->getSize();
		vector<std::uint8_t> pixels(static_cast<size_t>(size.x) * size.y * 4);
		glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		// GL rows start at the bottom of the window; PNG rows start at the top.
		const size_t rowBytes = static_cast<size_t>(size.x) * 4;
		for (unsigned int y = 0; y < size.y / 2; ++y) {
			std::swap_ranges(pixels.begin() + y * rowBytes, pixels.begin() + (y + 1) * rowBytes,
				pixels.begin() + (size.y - 1 - y) * rowBytes);
		}
		// The default framebuffer alpha is not meaningful; write opaque pixels.
		for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
		sf::Image image(size, pixels.data());
		if (!image.saveToFile(path)) {
			log(this, LogError, "Failed to save screenshot to '{}'", path);
		}
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
			root->queueRendering();
			if (environment.shadows) renderShadowMap(window);
			drawSky();
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

			if (!pendingScreenshotPath.empty()) {
				saveScreenshot(pendingScreenshotPath);
				pendingScreenshotPath.clear();
			}
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

	void Renderer::updateFrustum() {
		// Gribb-Hartmann: each plane is the last row of the clip matrix plus or minus another row.
		auto row = [this](int r) { return glm::vec4(cameraMatrix[0][r], cameraMatrix[1][r], cameraMatrix[2][r], cameraMatrix[3][r]); };
		const glm::vec4 last = row(3);
		frustumPlanes[0] = last + row(0);
		frustumPlanes[1] = last - row(0);
		frustumPlanes[2] = last + row(1);
		frustumPlanes[3] = last - row(1);
		frustumPlanes[4] = last + row(2);
		frustumPlanes[5] = last - row(2);
		for (glm::vec4& plane : frustumPlanes) {
			plane /= glm::length(glm::vec3(plane));
		}
	}

	void Renderer::collectVisibleInstances(const glm::mat4& bodyMatrix, const MeshData* meshData, const vector<glm::mat4>& instances) {
		visibleInstances.clear();
		for (const glm::mat4& instance : instances) {
			if (isVisible(bodyMatrix * instance, meshData->boundsMin, meshData->boundsMax)) {
				visibleInstances.push_back(instance);
			}
		}
		culledInstancesThisFrame += static_cast<unsigned int>(instances.size() - visibleInstances.size());
	}

	void Renderer::drawInstances(Mesh* mesh, MeshData* meshData, Program* program) {
		// One matrix per instance in a buffer, read as a per-instance attribute (divisor 1) on the mesh's own VAO.
		if (mesh->instanceBuffer == 0U) {
			GL_CHECK(glGenBuffers(1, &mesh->instanceBuffer));
		}
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, mesh->instanceBuffer));
		GL_CHECK(glBufferData(GL_ARRAY_BUFFER, visibleInstances.size() * sizeof(glm::mat4), visibleInstances.data(), GL_STREAM_DRAW));

		// A mat4 attribute takes four consecutive locations, one per column. A shader without it gets -1 and skips this.
		const GLint location = program->attrib("instanceMatrix");
		if (location >= 0) {
			for (GLint column = 0; column < 4; ++column) {
				GL_CHECK(glEnableVertexAttribArray(location + column));
				GL_CHECK(glVertexAttribPointer(location + column, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * column)));
				GL_CHECK(glVertexAttribDivisor(location + column, 1));
			}
		}

		const GLsizei count = static_cast<GLsizei>(visibleInstances.size());
		if (meshData->indexCount()) {
			GL_CHECK(glDrawElementsInstanced(GL_TRIANGLES, meshData->indexCount(), GL_UNSIGNED_INT, 0, count));
		} else {
			GL_CHECK(glDrawArraysInstanced(GL_TRIANGLES, 0, meshData->vertexCount(), count));
		}

		// Put the attribute back the way the mesh's own draws expect it: no array, no divisor.
		if (location >= 0) {
			for (GLint column = 0; column < 4; ++column) {
				GL_CHECK(glDisableVertexAttribArray(location + column));
				GL_CHECK(glVertexAttribDivisor(location + column, 0));
			}
		}
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
	}

	bool Renderer::isVisible(const glm::mat4& model, const glm::vec3& localMin, const glm::vec3& localMax) const {
		// Bounds of the transformed box. For a rotated box this is larger than the box itself, so the test is conservative.
		glm::vec3 worldMin(std::numeric_limits<float>::max());
		glm::vec3 worldMax(-std::numeric_limits<float>::max());
		for (int corner = 0; corner < 8; ++corner) {
			glm::vec3 local((corner & 1) ? localMax.x : localMin.x, (corner & 2) ? localMax.y : localMin.y, (corner & 4) ? localMax.z : localMin.z);
			glm::vec3 world = glm::vec3(model * glm::vec4(local, 1.0f));
			worldMin = glm::min(worldMin, world);
			worldMax = glm::max(worldMax, world);
		}
		for (const glm::vec4& plane : frustumPlanes) {
			// The corner furthest along the plane's normal. If it is behind the plane, the whole box is outside.
			glm::vec3 farthest((plane.x >= 0.0f) ? worldMax.x : worldMin.x, (plane.y >= 0.0f) ? worldMax.y : worldMin.y, (plane.z >= 0.0f) ? worldMax.z : worldMin.z);
			if (glm::dot(glm::vec3(plane), farthest) + plane.w < 0.0f) return false;
		}
		return true;
	}

	void Renderer::renderMesh(Mesh* mesh, MeshData* meshData, Transformation3D transform) {
		if (!meshData || meshData->vertexCount() == 0) return;	//Don;t log, null MeshData Meshes are empty Bodies
		if (!mesh) {
			log(this, LogError, "Mesh is invalid");
			return;
		}
		if (!mesh->getBodyId().has_value()) {
			log(this, LogError, "Mesh Body ID is empty in renderMesh");
			return;
		}

		// Skinned meshes are never culled or instanced: their bind-pose bounds do not cover the animated pose.
		const glm::mat4 bodyMatrix = getBodyGlobalTransform(mesh->getBodyId());
		const bool instanced = meshData->bones.empty() && !mesh->getInstances().empty();
		if (meshData->bones.empty()) {
			if (!meshData->boundsValid) meshData->computeBounds();
			if (instanced) {
				collectVisibleInstances(bodyMatrix, meshData, mesh->getInstances());
				if (visibleInstances.empty()) {
					++culledDrawsThisFrame;
					return;
				}
			} else if (!isVisible(bodyMatrix, meshData->boundsMin, meshData->boundsMax)) {
				++culledDrawsThisFrame;
				return;
			}
		}

		if (meshData->gpuDirty) uploadMeshData(meshData);

		// Only proceed with mesh rendering if we have mesh data
		if (meshData && meshData->vertexCount() > 0) {
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
			setTransformUBOData(transformUBOData, bodyMatrix);
			//If the mesh has bones, set the bone uniform values
			if (animator) {
				BoneUBO boneUBOData = BoneUBO();
				setBoneUBOData(boneUBOData, animator);
			}
			program->setUniform("cameraPosition", toGlm(currentCamera->getPosition()));
			program->setUniform("timeSec", time.getElapsedSec());
			program->setUniform("fogColour", toGlm(environment.fogColour));
			program->setUniform("fogRange", glm::vec2(environment.fogStart, environment.fogEnd));
			program->setUniform("instanced", instanced ? 1 : 0);
			// Static meshes do not enable the bone attributes, so those read defaults. Turning skinning off for them keeps
			// them from being moved by whatever bone matrices an animated model left in the bone buffer.
			program->setUniform("skinned", meshData->bones.empty() ? 0 : 1);
			// The shadow map: in its own pass the fragment shader stops early; otherwise surfaces look the sun's shadow up.
			program->setUniform("shadowPass", shadowPass ? 1 : 0);
			// Voxel meshes project their texture in world space (see triplanar in StdFragShader.frag).
			program->setUniform("triplanar", meshData->compact ? 1 : 0);
			program->setUniform("shadowsOn", (shadowsActive && !shadowPass) ? 1 : 0);
			if (shadowsActive && !shadowPass) {
				const GLint shadowUnit = 15;   // above the material textures' units
				GL_CHECK(glActiveTexture(GL_TEXTURE0 + shadowUnit));
				GL_CHECK(glBindTexture(GL_TEXTURE_2D, shadowDepthTexture));
				GL_CHECK(glActiveTexture(GL_TEXTURE0));
				program->setUniform("shadowMap", shadowUnit);
				program->setUniform("shadowMatrix", shadowMatrix);
				program->setUniform("shadowTexel", 1.0f / static_cast<float>(shadowResolution));
			}

			//Draw the MeshData by index, if available, or by vertices
			++drawCallsThisFrame;
			if (instanced) {
				drawInstances(mesh, meshData, program);
			} else if (meshData->indexCount()) {
				GL_CHECK(glDrawElements(GL_TRIANGLES, meshData->indexCount(), GL_UNSIGNED_INT, 0));
			} else {
				GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, meshData->vertexCount()));
			}

			// Unbind varray, shaders, and texture
			glBindVertexArray(0);
			glActiveTexture(GL_TEXTURE0);
			program->stop();
		}
	}

	void Renderer::endFrame() {
		updatedAnimators.clear();
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
		if (body->parentId.has_value() && body->parentId != world->getRoot()->getId()) {
			Body* parent = assets.get<Body>(body->parentId.value());
			if (!parent) return localTransform;
			glm::mat4 modelMatrix = getCombinedModelMatrix(parent) * localTransform;
			body->globalTransform = glmToTransform(modelMatrix);
			return modelMatrix;
		}

		return localTransform;
	};

	Animator* Renderer::updateAnimator(Mesh* mesh) {
		//Update each animator once per frame (instead of once per mesh), and return it for every mesh that uses it: each mesh
		//needs the bone matrices, not only the first one drawn.
		optional<id_t> meshModelId = mesh->getModelId();
		if (!meshModelId.has_value()) return nullptr;
		Model* meshModel = assets.get<Model>(meshModelId.value());
		if (!meshModel) return nullptr;
		// A model instance has its own animator (on its root body); a model made in code uses the model's.
		Animator* animator = nullptr;
		if (optional<id_t> bodyId = mesh->getBodyId()) {
			if (Body* body = assets.get<Body>(bodyId.value())) animator = body->findInstanceAnimator();
		}
		if (!animator) animator = meshModel->getAnimator();
		if (animator && updatedAnimators.insert(animator).second) {
			animator->updateAnimation(time.getDeltaSec());
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

	void Renderer::bindTextureAndSetUniform(Texture* texture, const string& paramName, Program* program, int materialIndex, int& boundTextures) {
		if (texture != nullptr) {
			GL_CHECK(glActiveTexture(GL_TEXTURE0 + boundTextures));
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture->getNativeHandle()));
			program->setUniform(getUniformArrayPropertyName("materialTextures", materialIndex, paramName).c_str(), boundTextures);
			boundTextures++;
		}
	}

	Renderer::MaterialFrameEntry& Renderer::getMaterialFrameEntry(id_t materialId, Material* material) {
		auto cached = materialFrameCache.find(materialId);
		if (cached != materialFrameCache.end()) return cached->second;

		MaterialFrameEntry& entry = materialFrameCache[materialId];
		auto textureParameter = [material](const char* paramName) -> Texture* {
			optional<ParamData> paramData = material->getParameter(paramName);
			return paramData.has_value() ? any_cast<Texture*>(paramData.value().data) : nullptr;
		};
		entry.diffuseTexture = textureParameter("diffuseTexture");
		entry.specularTexture = textureParameter("specularTexture");
		entry.opacityTexture = textureParameter("opacityTexture");

		optional<ParamData> paramData = nullopt;
		paramData = material->getParameter("diffuseColor");
		if (paramData.has_value()) entry.ubo.diffuseColor = glm::vec4(toGlm(any_cast<Color>(paramData.value().data)), 1);
		paramData = material->getParameter("diffuseTextureUVScale");
		if (paramData.has_value()) entry.ubo.diffuseTextureUVScale = toGlm(any_cast<Vector2f>(paramData.value().data));
		paramData = material->getParameter("diffuseTextureOffset");
		if (paramData.has_value()) entry.ubo.diffuseTextureOffset = toGlm(any_cast<Vector2f>(paramData.value().data));
		paramData = material->getParameter("diffuseTextureScrollSpeed");
		if (paramData.has_value()) entry.ubo.diffuseTextureScrollSpeed = toGlm(any_cast<Vector2f>(paramData.value().data));
		paramData = material->getParameter("specularColor");
		if (paramData.has_value()) entry.ubo.specularColor = glm::vec4(toGlm(any_cast<Color>(paramData.value().data)), 1);
		paramData = material->getParameter("specularTextureUVScale");
		if (paramData.has_value()) entry.ubo.specularTextureUVScale = toGlm(any_cast<Vector2f>(paramData.value().data));
		paramData = material->getParameter("specularTextureOffset");
		if (paramData.has_value()) entry.ubo.specularTextureOffset = toGlm(any_cast<Vector2f>(paramData.value().data));
		paramData = material->getParameter("specularTextureScrollSpeed");
		if (paramData.has_value()) entry.ubo.specularTextureScrollSpeed = toGlm(any_cast<Vector2f>(paramData.value().data));
		paramData = material->getParameter("opacityTextureUVScale");
		if (paramData.has_value()) entry.ubo.opacityTextureUVScale = toGlm(any_cast<Vector2f>(paramData.value().data));
		paramData = material->getParameter("opacityTextureOffset");
		if (paramData.has_value()) entry.ubo.opacityTextureOffset = toGlm(any_cast<Vector2f>(paramData.value().data));
		paramData = material->getParameter("opacityTextureScrollSpeed");
		if (paramData.has_value()) entry.ubo.opacityTextureScrollSpeed = toGlm(any_cast<Vector2f>(paramData.value().data));
		paramData = material->getParameter("smoothnessFactor");
		if (paramData.has_value()) entry.ubo.smoothnessFactor = any_cast<float>(paramData.value().data);
		paramData = material->getParameter("opacity");
		if (paramData.has_value()) entry.ubo.opacity = any_cast<float>(paramData.value().data);
		paramData = material->getParameter("alphaCutoff");
		if (paramData.has_value()) entry.ubo.alphaCutoff = any_cast<float>(paramData.value().data);
		paramData = material->getParameter("gamma");
		if (paramData.has_value()) entry.ubo.gamma = any_cast<float>(paramData.value().data);
		paramData = material->getParameter("opacityMasked");
		if (paramData.has_value()) entry.ubo.opacityMasked = any_cast<bool>(paramData.value().data);
		paramData = material->getParameter("useGammaCorrection");
		if (paramData.has_value()) entry.ubo.useGammaCorrection = any_cast<bool>(paramData.value().data);
		paramData = material->getParameter("useDiffuseTexture");
		if (paramData.has_value()) entry.ubo.useDiffuseTexture = any_cast<bool>(paramData.value().data);
		paramData = material->getParameter("useSpecularTexture");
		if (paramData.has_value()) entry.ubo.useSpecularTexture = any_cast<bool>(paramData.value().data);
		paramData = material->getParameter("useOpacityTexture");
		if (paramData.has_value()) entry.ubo.useOpacityTexture = any_cast<bool>(paramData.value().data);
		paramData = material->getParameter("useLighting");
		if (paramData.has_value()) entry.ubo.useLighting = any_cast<bool>(paramData.value().data);
		return entry;
	}

	void Renderer::setMaterialUBOData(MaterialUBO materialUBOData, vector<id_t> modelMaterials, Program* program) {
		static MaterialUBO previousMaterialUBOData;
		bool materialChanged = false;
		
		if (modelMaterials.size() > MaterialUBO::MAX_MATERIALS) {
			log(LogWarn, "Renderer", "Model has {} materials; only the first {} are used", modelMaterials.size(), MaterialUBO::MAX_MATERIALS);
		}
		const size_t materialCount = min(modelMaterials.size(), static_cast<size_t>(MaterialUBO::MAX_MATERIALS));
		for (size_t i = 0; i < materialCount; ++i) {
			Material* material = assets.get<Material>(modelMaterials.at(i));
			if (!material) continue;
			MaterialFrameEntry& entry = getMaterialFrameEntry(modelMaterials.at(i), material);
			//Bind the diffuse, specular and opacity textures, if any, and set the materialTextures[i] uniforms
			bindTextureAndSetUniform(entry.diffuseTexture, "diffuseTexture", program, i, boundTextures);
			bindTextureAndSetUniform(entry.specularTexture, "specularTexture", program, i, boundTextures);
			bindTextureAndSetUniform(entry.opacityTexture, "opacityTexture", program, i, boundTextures);
			materialUBOData.materials[i] = entry.ubo;
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

		vector<Light*> lights = assets.getAllResources<Light>();
		if (lights.size() > LightUBO::MAX_LIGHTS) {
			log(LogWarn, "Renderer", "Scene has {} lights; only the first {} are used", lights.size(), LightUBO::MAX_LIGHTS);
		}
		lightUBOData.lightCount = static_cast<int>(min(lights.size(), static_cast<size_t>(LightUBO::MAX_LIGHTS)));
		for (int i = 0; i < lightUBOData.lightCount; ++i) {
			Light* light = lights[i];
			lightUBOData.lights[i].position = light->position;
			lightUBOData.lights[i].brightness = light->parameters.brightness;
			lightUBOData.lights[i].intensities = glm::vec4(toGlm(light->parameters.colorIntensities), 1);
			lightUBOData.lights[i].attenuation = light->parameters.attenuation;
			lightUBOData.lights[i].ambiance = light->parameters.ambiance;
			lightUBOData.lights[i].coneAngle = light->parameters.coneAngle;
			lightUBOData.lights[i].lightDirection = glm::vec4(toGlm(light->parameters.lightDirection), 1);
		}
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
		transformUBOData.camera = cameraMatrix;
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

	void Renderer::add(id_t bodyId) {
		renderOrder.push_back(bodyId);
	}

	void Renderer::remove(id_t bodyId) {
		renderOrder.erase(std::remove(renderOrder.begin(), renderOrder.end(), bodyId), renderOrder.end());
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
		materialFrameCache.clear();
		lastFrameDrawCalls = drawCallsThisFrame;
		drawCallsThisFrame = 0;
		lastFrameCulledDraws = culledDrawsThisFrame;
		culledDrawsThisFrame = 0;
		lastFrameCulledInstances = culledInstancesThisFrame;
		culledInstancesThisFrame = 0;
	}
	
	void Renderer::sortZ() {
		sort(renderOrder.begin(), renderOrder.end(), [](id_t a, id_t b) { return (assets.get<Body>(a)->zOrder < assets.get<Body>(b)->zOrder); });
	}

	void Renderer::ensureShadowMap() {
		if (shadowFbo != 0) return;
		GL_CHECK(glGenTextures(1, &shadowDepthTexture));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, shadowDepthTexture));
		GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowResolution, shadowResolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
		const float lit[4] = { 1.0f, 1.0f, 1.0f, 1.0f };   // outside the map nothing is in shadow
		GL_CHECK(glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, lit));
		GL_CHECK(glGenFramebuffers(1, &shadowFbo));
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo));
		GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTexture, 0));
		GL_CHECK(glDrawBuffer(GL_NONE));
		GL_CHECK(glReadBuffer(GL_NONE));
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			log(this, LogError, "The shadow map framebuffer is incomplete");
		}
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	}

	// Draws every mesh from the sun's side into the shadow map. The map covers a square of shadowRadius on each side of the
	// camera: shadows near the viewer are sharp, and the far ones are simply lit (the fog covers them).
	void Renderer::renderShadowMap(RenderTarget* window) {
		Light* sun = nullptr;
		for (Light* light : assets.getAllResources<Light>()) {
			if (light->position.w == 0.0f) {   // a directional light
				sun = light;
				break;
			}
		}
		shadowsActive = false;
		if (!sun || !currentCamera) return;
		const glm::vec3 travel = glm::normalize(toGlm(sun->parameters.lightDirection));
		// Below the horizon the sun lights nothing, so there is no map to draw.
		if (travel.y > -0.02f) return;

		const glm::vec3 focus = toGlm(currentCamera->getPosition());
		const float radius = environment.shadowRadius;
		const double now = time.getElapsedSec();
		const bool moved = glm::distance(focus, shadowFocus) > radius * 0.05f;
		const bool turned = glm::distance(travel, shadowTravel) > 0.002f;
		const bool stale = now - shadowDrawnAt >= 0.1;
		if (shadowsDrawn && shadowRadiusDrawn == radius && !moved && !turned && !stale) {
			// The map still matches the matrix it was drawn with, which stays in shadowMatrix.
			shadowsActive = true;
			return;
		}
		ensureShadowMap();
		shadowsActive = true;
		shadowsDrawn = true;
		shadowDrawnAt = now;
		shadowFocus = focus;
		shadowTravel = travel;
		shadowRadiusDrawn = radius;

		const glm::vec3 up = std::fabs(travel.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::mat4 view = glm::lookAt(focus - travel * radius * 2.0f, focus, up);
		shadowMatrix = glm::ortho(-radius, radius, -radius, radius, 0.1f, radius * 4.0f) * view;

		const glm::mat4 cameraSaved = cameraMatrix;
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo));
		GL_CHECK(glViewport(0, 0, shadowResolution, shadowResolution));
		GL_CHECK(glEnable(GL_DEPTH_TEST));
		GL_CHECK(glDepthMask(GL_TRUE));
		GL_CHECK(glClear(GL_DEPTH_BUFFER_BIT));
		shadowPass = true;
		cameraMatrix = shadowMatrix;   // the transform block carries the sun's matrix, and culling uses its frustum
		updateFrustum();
		render(window);
		shadowPass = false;
		cameraMatrix = cameraSaved;
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
		const sf::Vector2u size = window->getSize();
		GL_CHECK(glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y)));
	}

	void Renderer::drawSky() {
		if (!skyProgram || !currentCamera || !environment.drawsSky()) return;
		GL_CHECK(glDisable(GL_DEPTH_TEST));
		GL_CHECK(glDepthMask(GL_FALSE));
		GL_CHECK(glUseProgram(skyProgram->getObjectId()));
		skyProgram->setUniform("camForward", currentCamera->getForward());
		skyProgram->setUniform("camRight", currentCamera->getRight());
		skyProgram->setUniform("camUp", currentCamera->getUp());
		skyProgram->setUniform("tanHalfFov", tanf(glm::radians(currentCamera->getFov()) * 0.5f));
		skyProgram->setUniform("aspect", currentCamera->getAspect());
		skyProgram->setUniform("horizonColour", toGlm(environment.skyColour));
		skyProgram->setUniform("zenithColour", toGlm(environment.zenithColour.value_or(environment.skyColour)));
		skyProgram->setUniform("sunDirection", glm::normalize(toGlm(environment.sunDirection)));
		skyProgram->setUniform("sunColour", toGlm(environment.sunColour));
		skyProgram->setUniform("sunCos", cosf(glm::radians(environment.sunSize)));
		GL_CHECK(glBindVertexArray(skyVao));
		GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));
		GL_CHECK(glBindVertexArray(0));
		GL_CHECK(glUseProgram(0));
		GL_CHECK(glDepthMask(GL_TRUE));
		GL_CHECK(glEnable(GL_DEPTH_TEST));
	}

	void Renderer::render(RenderTarget* window) {
		//The camera does not move during a render pass, so its matrix is computed once here, not for every draw
		if (currentCamera && !shadowPass) cameraMatrix = currentCamera->getMatrix();
		updateFrustum();

		//Sort the renderOrder by zOrder if zSorting is enabled
		if (zSortingEnabled) {
			sortZ();
		}

		//Draw each body with its calculated transform. Transparent bodies (the voxel water) come after the opaque ones, blended and
		//without writing depth, so what is behind them still shows. The shadow map skips them: they do not block the sun.
		vector<Body*> transparentBodies;
		for (auto iterator = renderOrder.begin(); iterator != renderOrder.end(); ++iterator) {
			Body* body = assets.get<Body>(*iterator);
			if (body->transparent) {
				if (!shadowPass) transparentBodies.push_back(body);
				continue;
			}
			body->onDraw(*window, body->getGlobalTransform());
		}
		if (!transparentBodies.empty()) {
			GL_CHECK(glDepthMask(GL_FALSE));
			for (Body* body : transparentBodies) {
				body->onDraw(*window, body->getGlobalTransform());
			}
			GL_CHECK(glDepthMask(GL_TRUE));
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
		//sf::Color channels are bytes; shaders take 0-1
		return glm::vec3(c.r, c.g, c.b) / 255.f;
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