#pragma once
// Renderer — 2D hex-grid map renderer using SDL2
//
// Reads from a MapSnapshot (thread-safe via mutex) and draws the map
// as colored rectangles (Milestone 2) or hexagons (Milestone 3).

#include <SDL.h>
#include "MapSnapshot.h"

struct Camera {
    float offsetX = 0.0f;  // world-space offset (pixels at zoom=1)
    float offsetY = 0.0f;
    float zoom = 1.0f;
};

class Renderer {
public:
    Renderer(SDL_Renderer* sdlRenderer, int windowW, int windowH);

    // Draw the map from the snapshot (locks the mutex briefly)
    void draw(MapSnapshot& snapshot);

    // Input handling
    void handleKeyDown(SDL_Keycode key, MapSnapshot& snapshot);
    void handleKeyUp(SDL_Keycode key);
    void handleMouseWheel(int y, int mouseX, int mouseY);
    void handleMouseMotion(int dx, int dy, bool middleButtonDown);
    void handleResize(int newW, int newH);

private:
    SDL_Renderer* m_renderer;
    int m_windowW, m_windowH;
    Camera m_camera;

    // Key state for smooth panning
    bool m_keyUp = false, m_keyDown = false, m_keyLeft = false, m_keyRight = false;
    float m_panSpeed = 400.0f; // pixels per second at zoom=1

    // Tile size in pixels (at zoom=1)
    static constexpr float TILE_SIZE = 24.0f;

    // Auto-fit camera on first frame with valid map data
    bool m_cameraInitialized = false;
    void autoFitCamera(const MapSnapshot& snapshot);

    void drawHUD(MapSnapshot& snapshot);
};
