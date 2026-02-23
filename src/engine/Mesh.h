#pragma once
// Mesh — GPU mesh data wrapper (VAO/VBO/IBO) for rendering NIF models
//
// Takes NIF geometry data (vertices, normals, UVs, triangles) and uploads
// to OpenGL. Supports textured rendering with basic material properties.

#include <glad.h>
#include <vector>
#include <string>
#include <cstdint>

struct MeshSubmesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    int indexCount = 0;
    GLuint textureID = 0;      // GL texture for this submesh (0 = no texture)
    float diffuseR = 0.8f, diffuseG = 0.8f, diffuseB = 0.8f;
    float alpha = 1.0f;
    bool hasAlphaBlend = false;
};

// Vertex layout: position(3) + normal(3) + uv(2) = 8 floats
struct MeshVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    // Non-copyable
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Move semantics
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    // Add a submesh from raw vertex/index data
    void addSubmesh(const std::vector<MeshVertex>& vertices,
                    const std::vector<uint16_t>& indices,
                    GLuint textureID = 0,
                    float diffR = 0.8f, float diffG = 0.8f, float diffB = 0.8f,
                    float alpha = 1.0f, bool alphaBlend = false);

    // Draw all submeshes
    void draw(GLuint shaderProgram) const;

    bool empty() const { return m_submeshes.empty(); }
    int submeshCount() const { return (int)m_submeshes.size(); }

private:
    std::vector<MeshSubmesh> m_submeshes;
    void cleanup();
};
