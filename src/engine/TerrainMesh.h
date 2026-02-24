#pragma once
// TerrainMesh — 3D terrain mesh generated from MapSnapshot tile data
//
// Each tile becomes a 4x4 subdivided quad (25 vertices, 32 triangles).
// Single VAO/VBO/IBO for the entire map. Rebuilt when map data changes.

#include <glad.h>
#include <vector>
#include <cstdint>

struct MapSnapshot;

// Per-vertex data for terrain rendering
struct TerrainVertex {
    float px, py, pz;   // position (Y = up)
    float nx, ny, nz;   // normal
    float r, g, b;       // color (M1: flat terrain color, later: replaced by splatmap)
};

class TerrainMesh {
public:
    TerrainMesh() = default;
    ~TerrainMesh();

    // Non-copyable
    TerrainMesh(const TerrainMesh&) = delete;
    TerrainMesh& operator=(const TerrainMesh&) = delete;

    // Rebuild mesh from map snapshot data. Call when map loads or visibility changes.
    void rebuild(const MapSnapshot& snapshot);

    // Draw the terrain mesh. Caller must have set up shader + VP uniforms.
    void draw() const;

    // Query terrain height at a world position (bilinear interpolation)
    // Returns 0.0 for M1 (flat terrain)
    float getHeight(float worldX, float worldZ) const;

    bool empty() const { return m_indexCount == 0; }

    // Map dimensions (set during rebuild)
    int mapWidth() const { return m_mapWidth; }
    int mapHeight() const { return m_mapHeight; }

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ibo = 0;
    int m_indexCount = 0;
    int m_mapWidth = 0;
    int m_mapHeight = 0;

    // Subdivision level per tile edge (4 = 4x4 grid = 25 verts per tile)
    static constexpr int SUBDIV = 4;

    void cleanup();
};
