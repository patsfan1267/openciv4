#pragma once
// MapSnapshot — Thread-safe snapshot of the game map for rendering
//
// The game thread writes this after each update().
// The render thread reads it each frame.
// Access is synchronized via the embedded mutex.

#include <vector>
#include <string>
#include <mutex>
#include <cstdint>
#include <queue>

// ---------- Command queue (render thread → game thread) ----------

struct GameCommand {
    enum Type {
        END_TURN,
        SELECT_UNIT,
        DESELECT,
        MOVE_UNIT,
        FOUND_CITY,
        SET_PRODUCTION,
        SET_RESEARCH,
        BUILD_IMPROVEMENT,
        FORTIFY,
        SLEEP,
        SKIP_TURN,
        CYCLE_UNIT,
        GOTO_PLOT,
        SELECT_CITY,
        CLOSE_CITY,
    };
    Type type;
    int id = -1;        // unit ID, city ID, or tech/building/unit type
    int x = -1, y = -1; // target coordinates
    int param = -1;     // extra parameter (build type, isUnit flag, etc.)
};

// ---------- Per-plot data ----------

struct PlotData {
    int terrainType;    // TerrainTypes enum value
    int plotType;       // PlotTypes: 0=PEAK, 1=HILLS, 2=LAND, 3=OCEAN
    int featureType;    // FeatureTypes enum value (-1 = none)
    int bonusType;      // BonusTypes enum value (-1 = none)
    int ownerID;        // PlayerTypes (-1 = no owner)
    bool isRiver;
    bool isNOfRiver;    // river along north edge of this plot
    bool isWOfRiver;    // river along west edge of this plot
    bool isCity;
    int unitCount;
    int cityPopulation; // 0 if not a city
    uint8_t ownerColorR, ownerColorG, ownerColorB; // owner's primary color
    std::string cityName;  // empty if not a city

    // Phase 2: per-plot unit data
    int firstUnitID = -1;       // ID of first unit on this plot (-1 if none)
    int firstUnitOwner = -1;    // owner of first unit
    bool hasHumanUnit = false;  // any unit owned by player 0?
    std::string firstUnitName;  // type name of first unit (e.g. "Warrior")
    int firstUnitHP = 100;      // health percentage 0-100
    int firstUnitMoves = 0;     // movement points remaining (x100)
    int firstUnitMaxMoves = 0;  // max movement points (x100)
    int firstUnitStrength = 0;  // combat strength (x100)
    bool firstUnitCanFound = false; // can this unit found a city?
    int improvementType = -1;   // ImprovementTypes (-1 = none)
    int visibility = 2;         // 0=unseen, 1=revealed (fog), 2=currently visible

    // City details (populated when a city is selected)
    int cityID = -1;

    // 3D rendering: building NIF path for first building in city (for M5)
    std::string cityBuildingNIF;  // NIF path of a representative building (e.g., Palace)
    float cityBuildingScale = 1.0f;

    // 3D rendering: unit NIF path for first unit on plot (for M6)
    std::string firstUnitNIF;    // NIF path (e.g., "Art/Units/Warrior/Warrior.nif")
    float firstUnitNIFScale = 1.0f;
};

// ---------- Production/tech items for UI panels ----------

struct ProductionItem {
    int type;           // UnitTypes or BuildingTypes
    bool isUnit;        // true = unit, false = building
    std::string name;
    int turns;          // turns to complete
};

struct TechItem {
    int techID;
    std::string name;
    int turnsLeft;
};

struct BuildItem {
    int buildType;      // BuildTypes index
    std::string name;
    int turnsLeft;
};

// ---------- City detail (populated on city selection) ----------

struct CityDetail {
    int cityID = -1;
    std::string name;
    int population = 0;
    int foodRate = 0;
    int productionRate = 0;
    int commerceRate = 0;
    int foodStored = 0;
    int foodNeeded = 0;
    std::string currentProduction;
    int productionTurns = 0;
    int productionStored = 0;
    int productionNeeded = 0;
    std::vector<ProductionItem> availableProduction;
    int garrisonCount = 0;
};

// ---------- Player info ----------

struct PlayerInfo {
    bool alive = false;
    bool isHuman = false;
    int numCities = 0;
    int numUnits = 0;
    int totalPop = 0;
    int score = 0;
    int gold = 0;
    int goldRate = 0;
    int scienceRate = 0;  // beakers per turn
    int currentResearch = -1;
    std::string currentResearchName;
    int researchTurns = 0;
    uint8_t colorR = 200, colorG = 200, colorB = 200;
    std::string civName;
};

// ---------- Map snapshot ----------

struct MapSnapshot {
    int width = 0;
    int height = 0;
    int gameTurn = 0;
    int gameYear = 0;
    bool wrapX = false;
    bool wrapY = false;
    bool paused = false;
    int turnDelayMs = 0;
    std::vector<PlotData> plots;  // size = width * height, row-major (y * width + x)
    PlayerInfo players[18];       // MAX_CIV_PLAYERS = 18
    int numPlayers = 0;
    std::mutex mtx;

    // Phase 2: selection + turn state
    int selectedUnitID = -1;
    int selectedUnitX = -1;
    int selectedUnitY = -1;
    bool isHumanTurn = false;
    bool waitingForEndTurn = false;
    int activePlayerID = -1;
    int humanPlayerID = 0;

    // City detail (populated when a city is selected)
    bool cityScreenOpen = false;
    CityDetail selectedCity;

    // Tech picker
    std::vector<TechItem> availableTechs;

    // Worker builds (for selected worker)
    std::vector<BuildItem> availableBuilds;

    // Game messages
    std::vector<std::string> gameMessages;  // recent messages for display

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
