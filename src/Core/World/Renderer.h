#pragma once
#define GLEW_STATIC
#include "GL/glew.h"
/// GLM is needed to provide 3D math properties, particularly matrices for 3D transformations.
#define GLM_ENABLE_EXPERIMENTAL
#include "glm.hpp" 
#include "gtx/transform.hpp"
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
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
using namespace Assimp;
using namespace std;
using namespace sf;
//#define MAX_BONE_INFLUENCE 4

namespace CGEngine {
	class Mesh;

	class VertexData {
	public:
		VertexData() {};
		VertexData(glm::vec3 position, glm::vec2 texCoord, glm::vec3 normal, GLfloat materialId, int* bones = {}, float* wgts = {}) : position(position), texCoord(texCoord), normal(normal), materialId(materialId) { };
		glm::vec3 position = { 0,0,0 };
		glm::vec2 texCoord = { 0,0 };
		glm::vec3 normal = { 0,0,0 };
		GLfloat materialId = 0;
	};

	struct TextureData {
		unsigned int id;
		string type;
		string path;
	};

	struct MeshData {
		MeshData(vector<VertexData> vertices = {}, vector<unsigned int> indices = {}, vector<Material*> materials = {new Material()}) :vertices(vertices), indices(indices), materials(materials), vao(0U), vbo(0U), ebo(0U) {};
		vector<VertexData> vertices;
		vector<unsigned int> indices;
		GLuint vbo = 0U;
		GLuint vao = 0U;
		GLuint ebo = 0U;
		vector<Material*> materials;

		GLint getCount() {
			return vertices.size() / 9.0f;
		}
		GLint getVertexLayoutSize() {
			return vertices.size() * sizeof(float);
		}
		GLint getIndexLayoutSize() {
			return indices.size() * sizeof(unsigned int);
		}
	};

	//struct BoneData {
	//	id_t id;
	//	glm::mat4 offset;
	//};

	/// <summary>
	/// Responsible for ordering Bodies for rendering. Allows for default ordering (children render on top of parents)
	/// modified with per-object Z-Order
	/// </summary>
	class Renderer {
	public:
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

		vector<MeshData> processNode(aiNode* node, const aiScene* scene, Mesh* mesh);

		bool processRender();
		void setWindow(RenderWindow* window);
		Camera* getCurrentCamera();
		void setCurrentCamera(Camera* camera);

		void renderMesh(MeshData model, Transformation3D transform);
		void getModelData(Mesh* mesh);
		id_t addLight(Light* light);
		void removeLight(id_t lightId);
		Light* getLight(id_t lightId);
		string getUniformArrayPropertyName(string arrayName, int index, string propertyName);
		string getUniformObjectPropertyName(string objectName, string propertyName);
		void setMaterialUniforms(Material* material, Program* program, int materialId = 0);
		void setLightUniforms(Light* light, size_t lightIndex, Program* program);
		glm::vec2 toGlm(Vector2f v);
		glm::vec3 toGlm(Vector3f v);
		glm::vec3 toGlm(Color c);
		Color fromAiColor4(aiColor4D* c);
		//static inline glm::mat4 fromAiMatrix4toGlm(const aiMatrix4x4& from) {
		//	glm::mat4 to;
		//	//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
		//	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
		//	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
		//	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
		//	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
		//	return to;
		//}
		//
		//static inline glm::vec3 fromAiVec3toGlm(const aiVector3D& vec) {
		//	return glm::vec3(vec.x, vec.y, vec.z);
		//}
		//
		//static inline glm::quat fromAiQuatToGlm(const aiQuaternion& pOrientation) {
		//	return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
		//}
	private:
		friend class World;
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

		Camera* currentCamera = nullptr;
		/// <summary>
		/// The order in which to draw bodies, with Bodies further back in the vector drawn on top of other Bodies. This is cleared and re-calculated each frame
		/// </summary>
		vector<Body*> renderOrder;
		
		Importer modelImporter = Importer();
		vector<MeshData> importModel(string path, Mesh* mesh, unsigned int options = aiProcess_Triangulate | aiProcess_FlipUVs);
		vector<string> loadMaterialTextures(aiMaterial* mat, aiTextureType type);
		GLenum initGlew();
		Program* program;
		UniqueDomain<id_t, Light*> lights = UniqueDomain<id_t, Light*>(10);
		int boundTextures = 0;

		//Fallback Material
		id_t fallbackMaterialId;
	};
}