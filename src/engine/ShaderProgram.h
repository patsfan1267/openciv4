#pragma once
// ShaderProgram — GLSL shader compilation, linking, and uniform setting

#include <glad.h>
#include <string>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    // Compile vertex + fragment shaders from source strings. Returns true on success.
    bool compile(const char* vertexSrc, const char* fragmentSrc);
    void destroy();

    void use() const;
    GLuint id() const { return m_program; }

    // Uniform setters
    GLint uniformLoc(const char* name) const;
    void setInt(const char* name, int v) const;
    void setFloat(const char* name, float v) const;
    void setVec2(const char* name, float x, float y) const;
    void setVec3(const char* name, float x, float y, float z) const;
    void setVec4(const char* name, float x, float y, float z, float w) const;
    void setMat4(const char* name, const float* m) const;

private:
    GLuint m_program = 0;
    static GLuint compileShader(GLenum type, const char* src);
};
