#pragma once
// AssetManager — Loads game art assets from Civ4's layered directory structure
//
// Asset search order: BTS -> Warlords -> Base game
// FPK archives are searched after loose files in each layer.
//
// Supports both SDL_Renderer textures (legacy 2D) and OpenGL textures.

#include <SDL.h>
#include <glad.h>
#include <string>
#include <memory>
#include <unordered_map>
#include "FPKArchive.h"
#include "DDSLoader.h"
#include "Mesh.h"

class AssetManager {
public:
    AssetManager() = default;
    ~AssetManager();

    // Initialize with SDL_Renderer (legacy 2D path)
    bool init(SDL_Renderer* renderer, const char* btsInstallDir);

    // Initialize for OpenGL (no SDL_Renderer needed)
    bool initGL(const char* btsInstallDir);

    // Get SDL terrain/feature textures (legacy 2D)
    SDL_Texture* getTerrainTexture(int terrainType) const;
    SDL_Texture* getFeatureTexture(int featureType) const;

    // Get OpenGL terrain/feature texture IDs (button icons)
    GLuint getTerrainTextureGL(int terrainType) const;
    GLuint getFeatureTextureGL(int featureType) const;

    // Get 512x512 blend terrain textures (for map rendering)
    GLuint getTerrainBlendGL(int terrainType) const;
    GLuint getFeatureBlendGL(int featureType) const;
    bool hasBlendTextures() const { return !m_terrainBlendGL.empty(); }

    // Procedural water textures (for ocean/coast tile fill)
    GLuint getOceanWaterGL() const { return m_oceanWaterGL; }
    GLuint getCoastWaterGL() const { return m_coastWaterGL; }

    // Check if textures are loaded
    bool hasTextures() const { return !m_terrainTextures.empty() || !m_terrainTexturesGL.empty(); }

    // Access to the FPK archive (for loading additional assets)
    FPKArchive& getArt0() { return m_art0; }

    // Load a DDS from FPK and return as GL texture ID (0 on failure)
    GLuint loadGLTextureFromFPK(const std::string& path);

    // Load RGBA pixel data from FPK DDS (for custom processing)
    bool loadDDSFromFPK(const std::string& path, DDSImage& out);

    // Load a 3D model from FPK NIF file, return cached Mesh (nullptr on failure)
    Mesh* getModel(const std::string& nifPath);

    // Resolve a texture path relative to a NIF file (handles .tga→.dds fallback)
    GLuint resolveNifTexture(const std::string& nifDir, const std::string& texName);

private:
    SDL_Renderer* m_renderer = nullptr;
    FPKArchive m_art0;

    // SDL textures (legacy 2D)
    std::unordered_map<int, SDL_Texture*> m_terrainTextures;
    std::unordered_map<int, SDL_Texture*> m_featureTextures;

    // OpenGL textures (button icons)
    std::unordered_map<int, GLuint> m_terrainTexturesGL;
    std::unordered_map<int, GLuint> m_featureTexturesGL;

    // OpenGL blend textures (512x512 seamless terrain)
    std::unordered_map<int, GLuint> m_terrainBlendGL;
    std::unordered_map<int, GLuint> m_featureBlendGL;

    // Procedural water textures
    GLuint m_oceanWaterGL = 0;
    GLuint m_coastWaterGL = 0;
    void generateWaterTextures();

    SDL_Texture* createTextureFromRGBA(const uint8_t* pixels, int w, int h);
    SDL_Texture* loadTextureFromFPK(const FPKArchive& fpk, const std::string& path);
    GLuint createGLTextureFromRGBA(const uint8_t* pixels, int w, int h);

    bool openArt0(const char* btsInstallDir);

    // Model cache (NIF path → GPU mesh)
    std::unordered_map<std::string, std::unique_ptr<Mesh>> m_modelCache;

    // Texture cache for NIF-referenced textures
    std::unordered_map<std::string, GLuint> m_nifTextureCache;
};
