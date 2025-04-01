#pragma once

#include <GL/glew.h>
#include <string>
#include <iostream>
#include "../Engine/EngineSystem.h"

using namespace std;
#define GL_CHECK(x) x; checkGLError(#x, __FILE__, __LINE__)

namespace CGEngine {
    enum class ShaderType {
        Vertex = GL_VERTEX_SHADER,
        Fragment = GL_FRAGMENT_SHADER,
        Geometry = GL_GEOMETRY_SHADER,
        Compute = GL_COMPUTE_SHADER,
        TessControl = GL_TESS_CONTROL_SHADER,
        TessEvaluation = GL_TESS_EVALUATION_SHADER
    };
    const string shaderTypes[6] = { "vertex","fragment","geometry","compute","tessellation control", "tesselation evaluation" };

	class Shader : public EngineSystem {
    public:
		static Shader readFile(const string& filePath, GLenum shaderType);
        Shader(const string& shaderCode, GLenum shaderType);

        GLuint getObjectId() const;

        Shader(const Shader& other);
        Shader& operator =(const Shader& other);
        ~Shader();
        bool isValid() const;
    private:
        string shaderTypeName;
        GLuint objectId;
        unsigned* refCount;

        void retain();
        void release();

        GLenum checkGLError(const char* operation, const char* file, int line);
	};
}