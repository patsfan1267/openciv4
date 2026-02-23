#pragma once
// Renderer — 2D square-grid map renderer using SDL2 + SDL2_ttf
//
// Renders the game map as colored squares with terrain colors,
// city markers, territory borders, river indicators, city name
// labels, feature overlays, HUD text, and a minimap.
// Civ4 uses a square tile grid (hexes were introduced in Civ5).

#include <SDL.h>
#include <SDL_ttf.h>
#include "MapSnapshot.h"
#include <string>

struct Camera {
    float offsetX = 0.0f;  // world-space offset (pixels at zoom=1)
    float offsetY = 0.0f;
    float zoom = 1.0f;
};

class Renderer {
public:
    Renderer(SDL_Renderer* sdlRenderer, int windowW, int windowH);
    ~Renderer();

    // Initialize font system (call once after construction)
    bool initFonts(const char* fontPath);

    // Draw the map from the snapshot (locks the mutex briefly)
    void draw(MapSnapshot& snapshot);

    // Input handling
    void handleKeyDown(SDL_Keycode key, MapSnapshot& snapshot);
    void handleKeyUp(SDL_Keycode key);
    void handleMouseWheel(int y, int mouseX, int mouseY);
    void handleMouseMotion(int dx, int dy, bool anyDragButton);
    void handleMouseClick(int mouseX, int mouseY, int button, MapSnapshot& snapshot);
    void handleResize(int newW, int newH);

private:
    SDL_Renderer* m_renderer;
    int m_windowW, m_windowH;
    Camera m_camera;

    // Key state for smooth panning
    bool m_keyUp = false, m_keyDown = false, m_keyLeft = false, m_keyRight = false;
    float m_panSpeed = 400.0f; // pixels per second at zoom=1

    // Delta-time tracking (milliseconds)
    Uint32 m_lastFrameTime = 0;

    // Toggle states
    bool m_showMinimap = true;
    bool m_showHelp = false;

    // Mouse position (for hover tooltip)
    int m_mouseX = 0, m_mouseY = 0;

    // Player panel / grid toggles
    bool m_showPlayerPanel = true;
    bool m_showGrid = false;

    // Square tile geometry constants (at zoom=1)
    static constexpr float TILE_SIZE = 24.0f;  // pixels per tile side

    // Auto-fit camera on first frame with valid map data
    bool m_cameraInitialized = false;
    void autoFitCamera(const MapSnapshot& snapshot);

    // Minimap bounds (updated each frame for click-to-jump)
    int m_mmX = 0, m_mmY = 0, m_mmW = 0, m_mmH = 0;

    // Tile drawing helpers
    void tileTopLeft(int col, int row, int mapHeight, float& tx, float& ty) const;
    void drawFilledTile(float tx, float ty, float size, uint8_t r, uint8_t g, uint8_t b);
    void drawTileOutline(float tx, float ty, float size, uint8_t r, uint8_t g, uint8_t b);
    void drawTileEdge(float tx, float ty, float size, int edge,
                      uint8_t r, uint8_t g, uint8_t b, int thickness);

    // Feature overlays (forests, jungle, etc.)
    void drawFeatureOverlay(float cx, float cy, float halfSize, int featureType);

    // Text rendering
    TTF_Font* m_fontSmall = nullptr;   // 11pt — city labels
    TTF_Font* m_fontMedium = nullptr;  // 14pt — HUD text
    void drawText(const std::string& text, int x, int y, TTF_Font* font,
                  uint8_t r, uint8_t g, uint8_t b);
    void drawTextCentered(const std::string& text, int cx, int cy, TTF_Font* font,
                          uint8_t r, uint8_t g, uint8_t b);

    // HUD overlay
    void drawHUD(MapSnapshot& snapshot);

    // Minimap
    void drawMinimap(const MapSnapshot& snapshot);

    // Help overlay
    void drawHelpOverlay();

    // Tooltip (hover info)
    void drawTooltip(const MapSnapshot& snapshot);

    // Player info panel
    void drawPlayerPanel(const MapSnapshot& snapshot);
};
