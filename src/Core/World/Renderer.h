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

namespace CGEngine {
	class Mesh;

	struct ModelData {
		ModelData() :vbo(0), vao(0), ebo(0), drawType(GL_TRIANGLES), drawStart(0), drawCount(0), materials({}) {};
		ModelData(GLint drawCount, vector<Material*> materials = { new Material() }, GLint drawStart = 0, GLenum type = GL_TRIANGLES) :vbo(0), vao(0), ebo(0), drawType(type), drawStart(drawStart), drawCount(drawCount), materials(materials) {};
		GLuint vbo;
		GLuint vao;
		GLuint ebo;
		GLenum drawType;
		GLint drawStart;
		GLint drawCount;
		vector<Material*> materials;
	};
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

		vector<VertexModel> processNode(aiNode* node, const aiScene* scene);

		bool processRender();
		void setWindow(RenderWindow* window);
		Camera* getCurrentCamera();
		void setCurrentCamera(Camera* camera);

		void renderMesh(VertexModel model, Transformation3D transform, ModelData data);
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
		vector<VertexModel> importModel(string path, unsigned int options = aiProcess_Triangulate | aiProcess_FlipUVs);
		GLenum initGlew();
		Program* program;
		UniqueDomain<id_t, Light*> lights = UniqueDomain<id_t, Light*>(10);
		int boundTextures = 0;
	};
}