#pragma once
// AssetManager — Loads game art assets from Civ4's layered directory structure
//
// Asset search order: BTS → Warlords → Base game
// FPK archives are searched after loose files in each layer.
//
// Phase 1b: terrain icon textures (64x64 DDS from Art0.FPK)

#include <SDL.h>
#include <string>
#include <unordered_map>
#include "FPKArchive.h"
#include "DDSLoader.h"

class AssetManager {
public:
    AssetManager() = default;
    ~AssetManager();

    // Initialize: open FPK archives and set up search paths.
    // btsInstallDir = path to "Beyond the Sword" directory.
    bool init(SDL_Renderer* renderer, const char* btsInstallDir);

    // Get the terrain texture for a given terrain type index.
    // Returns nullptr if not loaded.
    SDL_Texture* getTerrainTexture(int terrainType) const;

    // Get the feature texture for a given feature type index.
    SDL_Texture* getFeatureTexture(int featureType) const;

    // Check if textures are loaded
    bool hasTextures() const { return !m_terrainTextures.empty(); }

private:
    SDL_Renderer* m_renderer = nullptr;

    // FPK archives (base game)
    FPKArchive m_art0;

    // Terrain type → SDL_Texture (64x64 icons)
    std::unordered_map<int, SDL_Texture*> m_terrainTextures;

    // Feature type → SDL_Texture
    std::unordered_map<int, SDL_Texture*> m_featureTextures;

    // Helper: create an SDL_Texture from RGBA pixel data
    SDL_Texture* createTextureFromRGBA(const uint8_t* pixels, int w, int h);

    // Helper: load a DDS from the FPK, decompress, and create SDL_Texture
    SDL_Texture* loadTextureFromFPK(const FPKArchive& fpk, const std::string& path);
};
