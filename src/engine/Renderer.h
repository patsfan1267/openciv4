#pragma once
// Renderer — 2D square-grid map renderer using SDL2 + SDL2_ttf
//
// Renders the game map as colored squares with terrain colors,
// city markers, territory borders, river indicators, city name
// labels, feature overlays, HUD text, minimap, and gameplay UI.

#include <SDL.h>
#include <SDL_ttf.h>
#include "MapSnapshot.h"
#include "AssetManager.h"
#include <string>
#include <functional>

struct Camera {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float zoom = 1.0f;
};

// Callback for pushing game commands from the renderer
using CommandCallback = std::function<void(GameCommand)>;

class Renderer {
public:
    Renderer(SDL_Renderer* sdlRenderer, int windowW, int windowH, AssetManager* assets = nullptr);
    ~Renderer();

    bool initFonts(const char* fontPath);
    void draw(MapSnapshot& snapshot);

    // Set the callback for pushing game commands
    void setCommandCallback(CommandCallback cb) { m_pushCommand = cb; }

    // Input handling
    void handleKeyDown(SDL_Keycode key, MapSnapshot& snapshot);
    void handleKeyUp(SDL_Keycode key);
    void handleMouseWheel(int y, int mouseX, int mouseY);
    void handleMouseMotion(int dx, int dy, bool anyDragButton);
    void handleMouseClick(int mouseX, int mouseY, int button, MapSnapshot& snapshot);
    void handleResize(int newW, int newH);

    // Center camera on a world position
    void centerOnTile(int tileX, int tileY, int mapHeight);

private:
    SDL_Renderer* m_renderer;
    AssetManager* m_assets = nullptr;
    int m_windowW, m_windowH;
    Camera m_camera;
    CommandCallback m_pushCommand;

    // Key state for smooth panning
    bool m_keyUp = false, m_keyDown = false, m_keyLeft = false, m_keyRight = false;
    float m_panSpeed = 400.0f;
    Uint32 m_lastFrameTime = 0;

    // Toggle states
    bool m_showMinimap = true;
    bool m_showHelp = false;
    bool m_showPlayerPanel = true;
    bool m_showGrid = false;
    bool m_showTechPicker = false;
    int m_techScrollOffset = 0;

    // Mouse position
    int m_mouseX = 0, m_mouseY = 0;

    // Square tile geometry
    static constexpr float TILE_SIZE = 24.0f;

    bool m_cameraInitialized = false;
    void autoFitCamera(const MapSnapshot& snapshot);

    // Minimap bounds
    int m_mmX = 0, m_mmY = 0, m_mmW = 0, m_mmH = 0;

    // City production picker state
    int m_prodScrollOffset = 0;

    // Tile drawing
    void tileTopLeft(int col, int row, int mapHeight, float& tx, float& ty) const;
    void drawFilledTile(float tx, float ty, float size, uint8_t r, uint8_t g, uint8_t b);
    void drawTexturedTile(float tx, float ty, float size, SDL_Texture* tex);
    void drawTileOutline(float tx, float ty, float size, uint8_t r, uint8_t g, uint8_t b);
    void drawTileEdge(float tx, float ty, float size, int edge,
                      uint8_t r, uint8_t g, uint8_t b, int thickness);
    void drawFeatureOverlay(float cx, float cy, float halfSize, int featureType);

    // Text
    TTF_Font* m_fontSmall = nullptr;
    TTF_Font* m_fontMedium = nullptr;
    TTF_Font* m_fontLarge = nullptr;
    void drawText(const std::string& text, int x, int y, TTF_Font* font,
                  uint8_t r, uint8_t g, uint8_t b);
    void drawTextCentered(const std::string& text, int cx, int cy, TTF_Font* font,
                          uint8_t r, uint8_t g, uint8_t b);

    // UI panels
    void drawHUD(MapSnapshot& snapshot);
    void drawMinimap(const MapSnapshot& snapshot);
    void drawHelpOverlay();
    void drawTooltip(const MapSnapshot& snapshot);
    void drawPlayerPanel(const MapSnapshot& snapshot);
    void drawSelectionHighlight(float screenTX, float screenTY, float screenTileSize);
    void drawUnitPanel(const MapSnapshot& snapshot);
    void drawCityPanel(const MapSnapshot& snapshot);
    void drawTechPicker(const MapSnapshot& snapshot);
    void drawActionBar(const MapSnapshot& snapshot);
    void drawTurnBanner(const MapSnapshot& snapshot);
    void drawGameMessages(const MapSnapshot& snapshot);

    // Screen-to-tile coordinate conversion
    bool screenToTile(int screenX, int screenY, const MapSnapshot& snapshot, int& tileX, int& tileY);
};
