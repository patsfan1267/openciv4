#pragma once
// MapSnapshot — Thread-safe snapshot of the game map for rendering
//
// The game thread writes this after each turn.
// The render thread reads it each frame.
// Access is synchronized via the embedded mutex.

#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

struct PlotData {
    int terrainType;    // TerrainTypes enum value
    int plotType;       // PlotTypes: 0=PEAK, 1=HILLS, 2=LAND, 3=OCEAN
    int featureType;    // FeatureTypes enum value (-1 = none)
    int ownerID;        // PlayerTypes (-1 = no owner)
    bool isRiver;
    bool isCity;
    int unitCount;
    uint8_t ownerColorR, ownerColorG, ownerColorB; // owner's primary color
    std::string cityName;  // empty if not a city
};

struct MapSnapshot {
    int width = 0;
    int height = 0;
    int gameTurn = 0;
    int gameYear = 0;
    bool wrapX = false;
    bool wrapY = false;
    std::vector<PlotData> plots;  // size = width * height, row-major (y * width + x)
    std::mutex mtx;

    const PlotData& getPlot(int x, int y) const {
        return plots[y * width + x];
    }
};

// Terrain color mapping (RGB)
struct TerrainColor {
    uint8_t r, g, b;
};

// Returns a color for the given terrain type
inline TerrainColor getTerrainColor(int terrainType, int plotType) {
    // Plot type overrides for peaks
    if (plotType == 0) // PLOT_PEAK
        return {96, 96, 96};     // dark gray

    // Base terrain colors (indices match standard BTS terrain order)
    // TERRAIN_GRASS=0, PLAINS=1, DESERT=2, TUNDRA=3, SNOW=4, COAST=5, OCEAN=6, PEAK=7, HILL=8
    TerrainColor base;
    switch (terrainType) {
        case 0: base = {34, 139, 34};   break; // GRASS - forest green
        case 1: base = {210, 180, 100}; break; // PLAINS - tan
        case 2: base = {230, 210, 140}; break; // DESERT - sandy yellow
        case 3: base = {160, 160, 150}; break; // TUNDRA - gray
        case 4: base = {240, 240, 245}; break; // SNOW - white
        case 5: base = {100, 180, 220}; break; // COAST - light blue
        case 6: base = {30, 60, 140};   break; // OCEAN - dark blue
        case 7: base = {96, 96, 96};    break; // PEAK terrain type
        case 8: base = {160, 140, 90};  break; // HILL terrain type
        default: base = {128, 128, 128}; break;
    }

    // Darken hills by 20%
    if (plotType == 1) { // PLOT_HILLS
        base.r = (uint8_t)(base.r * 0.8f);
        base.g = (uint8_t)(base.g * 0.8f);
        base.b = (uint8_t)(base.b * 0.8f);
    }

    return base;
}
