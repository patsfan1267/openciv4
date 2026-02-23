#pragma once
// GLRenderer — OpenGL 3.3 map renderer + 2D UI overlay
//
// Replaces the SDL_Renderer-based Renderer with OpenGL.
// All drawing (map tiles, text, HUD, minimap, panels) goes through GL.

#include <glad.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "MapSnapshot.h"
#include "AssetManager.h"
#include "ShaderProgram.h"
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

// Camera and CommandCallback are defined in Renderer.h (shared between both renderers)
// If building without legacy renderer, define them here:
#ifndef RENDERER_H_CAMERA_DEFINED
struct Camera {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float zoom = 1.0f;
    float tiltAngle = 0.6f;     // radians, 0 = top-down, ~0.6 = ~35 deg tilt (isometric-ish)
    float rotationAngle = 0.0f; // radians, rotation around Y axis (0 = north-up)
};
using CommandCallback = std::function<void(GameCommand)>;
#endif

class GLRenderer {
public:
    GLRenderer();
    ~GLRenderer();

    // Initialize OpenGL state, shaders, and geometry. Call after GL context is current.
    bool init(int windowW, int windowH, AssetManager* assets);
    void shutdown();

    bool initFonts(const char* fontPath);
    void draw(MapSnapshot& snapshot);

    void setCommandCallback(CommandCallback cb) { m_pushCommand = cb; }
    void setDisable3D(bool v) { m_disable3D = v; }

    // Input handling (same API as old Renderer)
    void handleKeyDown(SDL_Keycode key, MapSnapshot& snapshot);
    void handleKeyUp(SDL_Keycode key);
    void handleMouseWheel(int y, int mouseX, int mouseY);
    void handleMouseMotion(int dx, int dy, bool rightDrag, bool middleDrag);
    void handleMouseClick(int mouseX, int mouseY, int button, MapSnapshot& snapshot);
    void handleResize(int newW, int newH);
    void centerOnTile(int tileX, int tileY, int mapHeight);

private:
    AssetManager* m_assets = nullptr;
    int m_windowW = 0, m_windowH = 0;
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
    bool m_disable3D = false; // --no-3d flag: skip 3D model rendering
    int m_techScrollOffset = 0;

    // Mouse
    int m_mouseX = 0, m_mouseY = 0;

    // Tile size
    static constexpr float TILE_SIZE = 24.0f;
    bool m_cameraInitialized = false;
    void autoFitCamera(const MapSnapshot& snapshot);

    // Minimap bounds
    int m_mmX = 0, m_mmY = 0, m_mmW = 0, m_mmH = 0;

    // City production picker state
    int m_prodScrollOffset = 0;

    // --- OpenGL resources ---
    ShaderProgram m_shader;   // unified color+texture shader (2D quads)
    ShaderProgram m_shader3D; // 3D model shader (MVP + lighting)

    // Dynamic quad batch: position(2) + uv(2) + color(4) = 8 floats per vertex
    GLuint m_batchVAO = 0, m_batchVBO = 0;
    std::vector<float> m_batchVerts;
    static constexpr int FLOATS_PER_VERT = 8; // x, y, u, v, r, g, b, a

    // A 1x1 white pixel texture for colored (non-textured) quads
    GLuint m_whiteTexture = 0;

    void initShaders();
    void initBatchBuffers();
    void initWhiteTexture();

    // Batch helpers
    void beginBatch();
    void pushQuad(float x, float y, float w, float h,
                  float r, float g, float b, float a,
                  GLuint tex = 0, float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1);
    void pushLine(float x1, float y1, float x2, float y2,
                  float thickness, float r, float g, float b, float a);
    // Push a quad with per-corner alpha (for gradient terrain blending)
    void pushQuadAlphaGrad(float x, float y, float w, float h,
                           float r, float g, float b,
                           float aTL, float aTR, float aBL, float aBR,
                           GLuint tex, float u0, float v0, float u1, float v1);
    void flushBatch();
    void setProjectionOrtho(); // set orthographic projection for current window size

    // Text rendering (SDL_ttf → GL texture)
    TTF_Font* m_fontSmall = nullptr;
    TTF_Font* m_fontMedium = nullptr;
    TTF_Font* m_fontLarge = nullptr;

    struct TextCacheEntry {
        GLuint textureID;
        int w, h;
    };
    std::unordered_map<std::string, TextCacheEntry> m_textCache;
    int m_textCacheFrame = 0; // cleared periodically to prevent unbounded growth

    GLuint getTextTexture(const std::string& text, TTF_Font* font,
                          uint8_t r, uint8_t g, uint8_t b, int& tw, int& th);
    void drawText(const std::string& text, int x, int y, TTF_Font* font,
                  uint8_t r, uint8_t g, uint8_t b);
    void drawTextCentered(const std::string& text, int cx, int cy, TTF_Font* font,
                          uint8_t r, uint8_t g, uint8_t b);

    // Tile/coordinate helpers
    void tileTopLeft(int col, int row, int mapHeight, float& tx, float& ty) const;
    bool screenToTile(int screenX, int screenY, const MapSnapshot& snapshot, int& tileX, int& tileY);

    // Drawing routines
    void drawTerrain(const MapSnapshot& snapshot);
    void draw3DModels(const MapSnapshot& snapshot);
    void drawSelectionHighlight(float screenTX, float screenTY, float screenTileSize);
    void drawHUD(MapSnapshot& snapshot);
    void drawMinimap(const MapSnapshot& snapshot);
    void drawHelpOverlay();
    void drawTooltip(const MapSnapshot& snapshot);
    void drawPlayerPanel(const MapSnapshot& snapshot);
    void drawUnitPanel(const MapSnapshot& snapshot);
    void drawCityPanel(const MapSnapshot& snapshot);
    void drawTechPicker(const MapSnapshot& snapshot);
    void drawActionBar(const MapSnapshot& snapshot);
    void drawTurnBanner(const MapSnapshot& snapshot);
    void drawGameMessages(const MapSnapshot& snapshot);
};
