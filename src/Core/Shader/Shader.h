#pragma once

#include <GL/glew.h>
#include <string>
#include <iostream>

using namespace std;

namespace CGEngine {
	class Shader {
    public:
		static Shader readFile(const string& filePath, GLenum shaderType);
        Shader(const string& shaderCode, GLenum shaderType);

        GLuint getObjectId() const;

        Shader(const Shader& other);
        Shader& operator =(const Shader& other);
        ~Shader();
    private:
        GLuint objectId;
        unsigned* refCount;

        void retain();
        void release();
	};
}