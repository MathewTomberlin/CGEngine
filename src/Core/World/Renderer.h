#pragma once
#define GLEW_STATIC
#include "GL/glew.h"
/// GLM is needed to provide 3D math properties, particularly matrices for 3D transformations.
#define GLM_ENABLE_EXPERIMENTAL
#include "glm.hpp" 
#include "gtx/transform.hpp"
#include "gtx/matrix_decompose.hpp"
#include "SFML/OpenGL.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <set>
#include <unordered_map>
#include <array>
#include <limits>
#include <iostream>
#include "../Shader/UniformBuffers.h"
#include "../Shader/Program.h"
#include "../Importer/MeshImporter.h"
#include "../Types/Types.h"
#include "../Camera/Camera.h"
#include "Environment.h"
using sf::Transform;
using sf::RenderTarget;
#define MAX_BONE_INFLUENCE 4
#define GL_CHECK(x) x; checkGLError(#x, __FILE__, __LINE__)

namespace CGEngine {
	class Mesh;
	class Bone;
	class Animation;
	class Animator;
	class Model;
	class Material;
	class Program;
	class Shader;
	class Body;
	class Light;

	struct VertexData {
		VertexData() {
			for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
				boneIds[i] = -1;
				weights[i] = 0.0f;
			}
		};
		VertexData(glm::vec3 position, glm::vec2 texCoord, glm::vec3 normal, GLfloat materialId) : position(position), texCoord(texCoord), normal(normal), materialId(materialId) {
			for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
				boneIds[i] = -1;
				weights[i] = 0.0f;
			}
		};
		glm::vec3 position = { 0,0,0 };
		glm::vec2 texCoord = { 0,0 };
		glm::vec3 normal = { 0,0,0 };
		GLfloat materialId = 0;
		int boneIds[MAX_BONE_INFLUENCE] = {-1,-1,-1,-1};
		float weights[MAX_BONE_INFLUENCE] = {};
		// Per-vertex tint, multiplied into the surface colour. White unless the mesh sets it (see create_mesh_body).
		glm::vec3 colour = glm::vec3(1.0f);
	};

	struct NodeData {
		glm::mat4 transformation;
		string name;
		int childrenCount;
		vector<NodeData> children;
	};

	struct TextureData {
		unsigned int id;
		string type;
		string path;
	};

	struct BoneData {
		BoneData() :id(0), offset(glm::mat4(1.0f)) {};
		BoneData(unsigned int id, glm::mat4 offset) :id(id), offset(offset) {};
		unsigned int id;
		glm::mat4 offset;
		bool operator==(const BoneData& other) const {
			return (id == id && offset == other.offset);
		}
	};

	// A vertex of a voxel mesh: 48 bytes, against 80 for VertexData. It leaves out the bone fields, and every attribute
	// is a float, since the driver reads byte attributes slowly. The shader sees the same attributes (see
	// Renderer::createCompactMeshBuffers).
	struct VoxelVertex {
		glm::vec3 position = { 0,0,0 };
		glm::vec2 texCoord = { 0,0 };
		glm::vec3 normal = { 0,0,0 };
		float materialId = 0.0f;
		glm::vec3 colour = { 1,1,1 };
	};

	struct MeshData : public IResource {
		MeshData(string meshName = "", vector<VertexData> vertices = {}, vector<unsigned int> indices = {}, map<string, BoneData> bones = {}) : meshName(meshName), vertices(vertices), indices(indices), vao(0U), vbo(0U), ebo(0U), bones(bones) {};
		MeshData(vector<VertexData> vertices, vector<unsigned int> indices = {}, map<string, BoneData> bones = {}, string meshName = "") : meshName(meshName), vertices(vertices), indices(indices), vao(0U), vbo(0U), ebo(0U), bones(bones) {};
		// A compact voxel mesh. The asset manager checks a resource when it is created, so its vertices are given here.
		MeshData(vector<VoxelVertex> voxels, vector<unsigned int> indices) : voxelVertices(voxels), compact(true), indices(indices), vao(0U), vbo(0U), ebo(0U) {};
		bool isValid() const {
			return vertexCount() > 0 && indexCount() > 0;
		}
		// Voxel meshes keep their vertices in voxelVertices (compact), everything else in vertices. Once a runtime voxel mesh
		// is on the GPU its arrays are freed (releaseCpuCopy), and the counts it had are kept.
		size_t vertexCount() const {
			if (!compact) return vertices.size();
			return arraysFreed ? gpuVertexCount : voxelVertices.size();
		}
		size_t indexCount() const {
			return arraysFreed ? gpuIndexCount : indices.size();
		}
		void releaseCpuCopy() {
			if (!compact || !runtimeMesh || arraysFreed) return;
			if (!boundsValid) computeBounds();
			gpuVertexCount = voxelVertices.size();
			gpuIndexCount = indices.size();
			vector<VoxelVertex>().swap(voxelVertices);
			vector<unsigned int>().swap(indices);
			arraysFreed = true;
		}
		string sourcePath = "";
		string meshName = "";
		vector<VertexData> vertices;
		vector<VoxelVertex> voxelVertices;
		bool compact = false;
		bool arraysFreed = false;       // the arrays were released after upload; see releaseCpuCopy
		size_t gpuVertexCount = 0;
		size_t gpuIndexCount = 0;
		vector<unsigned int> indices;
		map<string, BoneData> bones;
		// Box around the local-space positions, for culling. Recomputed when boundsValid is false.
		glm::vec3 boundsMin = glm::vec3(0.0f);
		glm::vec3 boundsMax = glm::vec3(0.0f);
		bool boundsValid = false;
		// Runtime meshes are made from Python (create_mesh_body). Only these may be changed after upload.
		bool runtimeMesh = false;
		// Set when vertices or indices change; the renderer re-uploads the buffers before the next draw.
		bool gpuDirty = false;
		void computeBounds() {
			boundsMin = glm::vec3(std::numeric_limits<float>::max());
			boundsMax = glm::vec3(-std::numeric_limits<float>::max());
			if (compact) {
				for (const VoxelVertex& vertex : voxelVertices) {
					boundsMin = glm::min(boundsMin, vertex.position);
					boundsMax = glm::max(boundsMax, vertex.position);
				}
			} else {
				for (const VertexData& vertex : vertices) {
					boundsMin = glm::min(boundsMin, vertex.position);
					boundsMax = glm::max(boundsMax, vertex.position);
				}
			}
			boundsValid = vertexCount() > 0;
		}
		GLuint vbo = 0U;
		GLuint vao = 0U;
		GLuint ebo = 0U;
	};

	struct KeyPosition {
		glm::vec3 position;
		float timeStamp;
	};

	struct KeyRotation {
		glm::quat orientation;
		float timeStamp;
	};

	struct KeyScale {
		glm::vec3 scale;
		float timeStamp;
	};

	/// <summary>
	/// Responsible for ordering Bodies for rendering. Allows for default ordering (children render on top of parents)
	/// modified with per-object Z-Order
	/// </summary>
	class Renderer : public EngineSystem {
	public:
		Renderer::Renderer();

		~Renderer() {
			glDeleteBuffers(1, &materialUBO);
			glDeleteBuffers(1, &lightUBO);
			glDeleteBuffers(1, &boneUBO);
			glDeleteBuffers(1, &transformUBO);
		}
		/// <summary>
		/// Add the Body to the renderOrder and and it and its transform to the bodyTransform map for this frame
		/// </summary>
		/// <param name="body">The Body to add to the Renderer</param>
		void add(id_t body);
		/// Remove a body from this frame's render order. Called when a Body is destroyed, so a removed Body
		/// is never drawn or ray-cast later in the same frame.
		void remove(id_t body);
		/// <summary>
		/// Calculate the greatest Z-Order of Bodies
		/// </summary>
		/// <returns>The greatest Z-Order among Bodies in the renderOrder</returns>
		int zMax();
		/// <summary>
		/// Calculate the minimum Z-Order of Bodies
		/// </summary>
		/// <returns>The minimum Z-Order among Bodies in the renderOrder</returns>
		int zMin();
		/// <summary>
		/// Get a vector of all bodies with the indicated Z-Order
		/// </summary>
		/// <param name="zIndex">The Z index to get Bodies at</param>
		/// <returns>A vector of Body pointers at the indicated Z index</returns>
		vector<id_t> getZBodies(int zIndex);
		/// <summary>
		/// Get a vector of all bodies below the indicated Z-Order
		/// </summary>
		/// <param name="zIndex">The Z index to get Bodies below</param>
		/// <returns>A vector of Body pointers below the indicated Z index</returns>
		vector<id_t> getLowerZBodies(int zIndex);
		/// <summary>
		/// Get a vector of all bodies above the indicated Z-Order
		/// </summary>
		/// <param name="zIndex">The Z index to get Bodies above</param>
		/// <returns>A vector of Body pointers above the indicated Z index</returns>
		vector<id_t> getHigherZBodies(int zIndex);

		void initializeOpenGL();
		bool setGLWindowState(bool state);
		bool clearGL(GLbitfield mask);

		// The sky colour and fog for the frames drawn next. Scenes set it with "environment", scripts with cge.set_environment.
		Environment environment;

		void commitGL();
		void pullGL();

		bool processRender();
		/// Number of glDraw calls issued in the last rendered frame.
		unsigned int getLastFrameDrawCalls() const { return lastFrameDrawCalls; }
		/// Meshes skipped in the last rendered frame because their bounds were outside the camera's view.
		unsigned int getLastFrameCulledDraws() const { return lastFrameCulledDraws; }
		/// Instances skipped in the last rendered frame because they were outside the view.
		unsigned int getLastFrameCulledInstances() const { return lastFrameCulledInstances; }
		void saveScreenshot(const std::string& path);
		/// Save the next presented frame to a PNG at path. Taken before the buffer swap.
		void requestScreenshot(const std::string& path) { pendingScreenshotPath = path; }
		void setWindow(RenderWindow* window);
		RenderWindow* getWindow() const { return window; }
		Camera* getCurrentCamera();
		void setCurrentCamera(unique_ptr<Camera> camera);

		void renderMesh(Mesh* mesh, MeshData* meshData, Transformation3D transform);
		void getModelData(Mesh* mesh);
		void uploadMeshData(MeshData* meshData);
		void createCompactMeshBuffers(MeshData* meshData, Program* program);
		void releaseMeshData(MeshData* meshData);
		string getUniformArrayIndexName(string arrayName, int index);
		string getUniformArrayPropertyName(string arrayName, int index, string propertyName);
		string getUniformObjectPropertyName(string objectName, string propertyName);
		void setMaterialUniforms(id_t materialAssetId, Program* program, int materialId = 0);
		void setLightUniforms(Light* light, size_t lightIndex, Program* program);
		glm::vec2 toGlm(Vector2f v);
		glm::vec3 toGlm(Vector3f v);
		glm::vec3 toGlm(Color c);
		Vector2f fromGlm(glm::vec2 v);
		Vector3f fromGlm(glm::vec3 v);
		Transform glmToTransform(const glm::mat4& mat) {
			return Transform(
				mat[0][0], mat[1][0], mat[3][0],
				mat[0][1], mat[1][1], mat[3][1],
				mat[0][3], mat[1][3], mat[3][3]
			);
		}
		const aiScene* readFile(string path, unsigned int options);
		ImportResult import(string path, const string& skeletonName="");
		Material* getFallbackMaterial();
		glm::mat4 getCombinedModelMatrix(Body* body);
		void endFrame();
	private:
		friend class World;
		/// <summary>
		/// Observation pointer of the RenderWindow owned by Screen
		/// </summary>
		RenderWindow* window = nullptr;
		string pendingScreenshotPath;
		bool glWindowActive = false;
		bool sfmlGLStatesActive = false;	//True while SFML's GL states are pushed (see commitGL/pullGL)
		unsigned int drawCallsThisFrame = 0;
		unsigned int lastFrameDrawCalls = 0;
		unsigned int culledDrawsThisFrame = 0;
		unsigned int lastFrameCulledDraws = 0;
		unsigned int culledInstancesThisFrame = 0;
		unsigned int lastFrameCulledInstances = 0;
		// Instances of the mesh being drawn that are inside the view (filled by collectVisibleInstances).
		vector<glm::mat4> visibleInstances;
		void collectVisibleInstances(const glm::mat4& bodyMatrix, const MeshData* meshData, const vector<glm::mat4>& instances);
		void drawInstances(Mesh* mesh, MeshData* meshData, Program* program);
		// Planes of the view frustum (normal.xyz, distance w; points with dot(normal, p) + w >= 0 are inside), set each frame.
		std::array<glm::vec4, 6> frustumPlanes{};
		void updateFrustum();
		bool isVisible(const glm::mat4& model, const glm::vec3& localMin, const glm::vec3& localMax) const;
		/// <summary>
		/// Clear the renderOrder and bodyTransform map
		/// </summary>
		void clear();
		/// <summary>
		/// Sort the Bodies by their Z-Order
		/// </summary>
		void sortZ();
		/// <summary>
		/// Draw the Bodies based on their Z-Order (or default order)
		/// </summary>
		/// <param name="window">The RenderTarget to draw the Body in</param>
		void render(RenderTarget* window);
		/// Draws the sky behind everything: a gradient from the horizon to the zenith, and the sun's disc. Drawn after the clear and before the bodies.
		void drawSky();
		std::unique_ptr<Program> skyProgram;
		GLuint skyVao = 0;

		/// <summary>
		/// The current render camera. This is set during OpenGL initialization and used to set the view matrix for the shader program.
		/// </summary>
		unique_ptr<Camera> currentCamera = nullptr;
		glm::mat4 cameraMatrix = glm::mat4(1.0f);	//currentCamera's matrix for the frame being rendered
		/// <summary>
		/// The order in which to draw bodies, with Bodies further back in the vector drawn on top of other Bodies. This is cleared and re-calculated each frame
		/// </summary>
		vector<id_t> renderOrder;
		/// <summary>
		/// If enabled, Bodies are sorted by their zOrder before rendering each frame.
		/// </summary>
		bool zSortingEnabled = true;
		set<id_t> updatedModels;
		GLenum initGlew();
		Program* program;
		int boundTextures = 0;
		/// <summary>
		/// A material's UBO entry and texture handles, read once per frame: several draws share a material, and each
		/// parameter read is a string lookup. Cleared in clear().
		/// </summary>
		struct MaterialFrameEntry {
			MaterialUBO::Material ubo{};
			Texture* diffuseTexture = nullptr;
			Texture* specularTexture = nullptr;
			Texture* opacityTexture = nullptr;
		};
		unordered_map<id_t, MaterialFrameEntry> materialFrameCache;
		MaterialFrameEntry& getMaterialFrameEntry(id_t materialId, Material* material);

		//Fallback Material
		id_t fallbackMaterialId;

		MeshImporter* importer;

		GLenum checkGLError(const char* operation, const char* file, int line);
		bool validateShader(Shader* shader);
		bool validateProgram(Program* program);
		glm::mat4 getBodyGlobalTransform(optional<id_t> bodyId);

		// UBOs
		GLuint materialUBO;
		GLuint lightUBO;
		GLuint boneUBO;
		GLuint transformUBO;

		void updateMaterialUBO(const MaterialUBO& materialData);
		void updateLightUBO(const LightUBO& lightData);
		void updateBoneUBO(const BoneUBO& boneData);
		void updateTransformUBO(const TransformUBO& transformData);
		void bindTextureAndSetUniform(Texture* texture, const string& paramName, Program* program, int materialIndex, int& boundTextures);
		void setMaterialUBOData(MaterialUBO materialUBOData, vector<id_t> modelMaterials, Program* program);
		void setLightUBOData(LightUBO lightUBOData);
		void setBoneUBOData(BoneUBO boneUBOData, Animator* animator);
		void setTransformUBOData(TransformUBO transformUBOData, glm::mat4 combinedTransform);
		Animator* updateAnimator(Mesh* mesh);
		Program* useRenderProgram(Material* renderMaterial);
	};
}