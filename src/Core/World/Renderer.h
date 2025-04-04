#pragma once
#define GLEW_STATIC
#include "GL/glew.h"
/// GLM is needed to provide 3D math properties, particularly matrices for 3D transformations.
#define GLM_ENABLE_EXPERIMENTAL
#include "glm.hpp" 
#include "gtx/transform.hpp"
#include "gtx/matrix_decompose.hpp"
#include "SFML/OpenGL.hpp"
#include <vector>
#include <map>
#include <string>
#include "../Body/Body.h"
#include "../Camera/Camera.h"
#include "../Shader/Shader.h"
#include "../Shader/Program.h"
#include "../Light/Light.h"
#include "../Material/Material.h"
#include "../Importer/MeshImporter.h"
using namespace Assimp;
using namespace std;
using namespace sf;
#define MAX_BONE_INFLUENCE 4
#define GL_CHECK(x) x; checkGLError(#x, __FILE__, __LINE__)

namespace CGEngine {
	class Mesh;
	class Bone;
	class Animation;
	class Animator;
	class Model;

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

	struct MeshData : public IResource {
		MeshData(string meshName = "", vector<VertexData> vertices = {}, vector<unsigned int> indices = {}, map<string, BoneData> bones = {}) : meshName(meshName), vertices(vertices), indices(indices), vao(0U), vbo(0U), ebo(0U), bones(bones) {};
		MeshData(vector<VertexData> vertices, vector<unsigned int> indices = {}, map<string, BoneData> bones = {}, string meshName = "") : meshName(meshName), vertices(vertices), indices(indices), vao(0U), vbo(0U), ebo(0U), bones(bones) {};
		bool isValid() const {
			return !vertices.empty() && !indices.empty();
		}
		string sourcePath = "";
		string meshName = "";
		vector<VertexData> vertices;
		vector<unsigned int> indices;
		map<string, BoneData> bones;
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
		Renderer::Renderer() {
			init();
			importer = new MeshImporter();
		}
		/// <summary>
		/// Add the Body to the renderOrder and and it and its transform to the bodyTransform map for this frame
		/// </summary>
		/// <param name="body">The Body to add to the Renderer</param>
		/// <param name="transform">The transform of the Body</param>
		void add(Body* body, Transform transform);
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
		vector<Body*> getZBodies(int zIndex);
		/// <summary>
		/// Get a vector of all bodies below the indicated Z-Order
		/// </summary>
		/// <param name="zIndex">The Z index to get Bodies below</param>
		/// <returns>A vector of Body pointers below the indicated Z index</returns>
		vector<Body*> getLowerZBodies(int zIndex);
		/// <summary>
		/// Get a vector of all bodies above the indicated Z-Order
		/// </summary>
		/// <param name="zIndex">The Z index to get Bodies above</param>
		/// <returns>A vector of Body pointers above the indicated Z index</returns>
		vector<Body*> getHigherZBodies(int zIndex);

		void initializeOpenGL();
		bool setGLWindowState(bool state);
		bool clearGL(GLbitfield mask);
		void commitGL();
		void pullGL();

		bool processRender();
		void setWindow(RenderWindow* window);
		Camera* getCurrentCamera();
		void setCurrentCamera(unique_ptr<Camera> camera);

		void renderMesh(Mesh* mesh, MeshData* meshData, Transformation3D transform);
		void getModelData(Mesh* mesh);
		void updateModelData(Mesh* mesh);
		string getUniformArrayIndexName(string arrayName, int index);
		string getUniformArrayPropertyName(string arrayName, int index, string propertyName);
		string getUniformObjectPropertyName(string objectName, string propertyName);
		void setMaterialUniforms(Material* material, Program* program, int materialId = 0);
		void setLightUniforms(Light* light, size_t lightIndex, Program* program);
		glm::vec2 toGlm(Vector2f v);
		glm::vec3 toGlm(Vector3f v);
		glm::vec3 toGlm(Color c);
		Vector2f fromGlm(glm::vec2 v);
		Vector3f fromGlm(glm::vec3 v);
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

		unique_ptr<Camera> currentCamera = nullptr;
		/// <summary>
		/// The order in which to draw bodies, with Bodies further back in the vector drawn on top of other Bodies. This is cleared and re-calculated each frame
		/// </summary>
		vector<Body*> renderOrder;
		/// <summary>
		/// If enabled, Bodies are sorted by their zOrder before rendering each frame.
		/// </summary>
		bool zSortingEnabled = true;
		std::set<Model*> updatedModels;
		GLenum initGlew();
		Program* program;
		int boundTextures = 0;

		//Fallback Material
		id_t fallbackMaterialId;

		MeshImporter* importer;

		GLenum checkGLError(const char* operation, const char* file, int line);
		bool validateShader(Shader* shader);
		bool validateProgram(Program* program);

	};
}