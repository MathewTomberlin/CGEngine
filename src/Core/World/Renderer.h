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
#include "../Body/Body.h"
#include "../Camera/Camera.h"
using namespace std;

namespace CGEngine {
	///Shader Types
	enum class ShaderType { Vertex, Fragment, Geometry, Count };
	///Standard Uniforms in the shader.
	enum class UniformType { TransformPVM, Count };
	///Vertex attributes for shaders and the input vertex array.
	enum class VertexAttribute { Position, TexCoord, COUNT };
	///Basic vertex shader that transforms the vertex position based on a projection view matrix and passes the texture coordinate to the fragment shader.
	const string defaultVertexShader =
		"#version 330\n"\
		"attribute vec3 position;"\
		"attribute vec2 texCoord;" \
		"uniform mat4 pvm;" \

		"varying vec2 uv;" \

		"void main() {"\
		"	gl_Position = pvm * vec4(position, 1.0);"\
		"	uv = texCoord;"\
		"}";

	///Basic fragment shader that returns the colour of a pixel based on the input texture and its coordinate.
	const string defaultFragShader =
		"#version 330\n" \
		"uniform sampler2D texture;" \
		"varying vec2 uv;" \

		"void main() {" \
		"	gl_FragColor = texture2D(texture, uv);" \
		"}";
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

		bool processRender();
		void setWindow(RenderWindow* window);
		Camera* getCurrentCamera();
		void setCurrentCamera(Camera* camera);

		void renderMesh(VertexModel model, Transformation3D transform);
		void bufferMeshData(VertexModel model);
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
		
		GLenum initGlew();

		///Shader Program
		GLuint program = 0;
		///List of shaders set up for a 3D scene.
		GLuint shader[static_cast<unsigned int>(ShaderType::Count)];
		///List of uniforms that can be defined values for the shader.
		GLint uniform[static_cast<unsigned int>(UniformType::Count)];
		//Shader functions
		void loadFromMemory(const string& shaderData, ShaderType type);
		GLuint buildShader(const string& l_src, unsigned int l_type);
		void checkError(GLuint l_shader, GLuint l_flag, bool l_program, const string& l_errorMsg);

		//Vertex Array and Buffer and Index Buffer
		GLuint vao = 0;
		GLuint vertexVBO = 0;
		GLuint indexVBO = 0;
		
		//View projection from screen
		glm::mat4 projection;
	};
}