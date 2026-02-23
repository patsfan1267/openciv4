// Renderer.cpp — 2D map renderer implementation
//
// Milestone 2: Colored rectangle grid based on terrain type
// (Milestone 3 will upgrade this to hex geometry)

#include "Renderer.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

Renderer::Renderer(SDL_Renderer* sdlRenderer, int windowW, int windowH)
    : m_renderer(sdlRenderer)
    , m_windowW(windowW)
    , m_windowH(windowH)
{
}

void Renderer::autoFitCamera(const MapSnapshot& snapshot)
{
    // Calculate zoom level that fits the entire map in the window
    // with a small margin (90% of window)
    float mapPixelW = snapshot.width * TILE_SIZE;
    float mapPixelH = snapshot.height * TILE_SIZE;

    float zoomX = (m_windowW * 0.9f) / mapPixelW;
    float zoomY = (m_windowH * 0.9f) / mapPixelH;
    m_camera.zoom = std::min(zoomX, zoomY);
    m_camera.zoom = std::max(0.1f, std::min(m_camera.zoom, 20.0f));

    // Center the map in the window
    float tileW = TILE_SIZE * m_camera.zoom;
    float tileH = TILE_SIZE * m_camera.zoom;
    float totalW = snapshot.width * tileW;
    float totalH = snapshot.height * tileH;

    // offsetX/Y are in world-space (pre-zoom), so divide by zoom
    m_camera.offsetX = -(m_windowW - totalW) / (2.0f * m_camera.zoom);
    m_camera.offsetY = -(m_windowH - totalH) / (2.0f * m_camera.zoom);

    fprintf(stderr, "[renderer] Auto-fit: map=%dx%d zoom=%.2f offset=(%.0f,%.0f)\n",
            snapshot.width, snapshot.height, m_camera.zoom,
            m_camera.offsetX, m_camera.offsetY);
}

void Renderer::draw(MapSnapshot& snapshot)
{
    // Update camera position from held keys
    // (called each frame, so scale by ~16ms assuming 60fps)
    float dt = 1.0f / 60.0f;
    float panDelta = m_panSpeed * dt / m_camera.zoom;
    if (m_keyUp)    m_camera.offsetY -= panDelta;
    if (m_keyDown)  m_camera.offsetY += panDelta;
    if (m_keyLeft)  m_camera.offsetX -= panDelta;
    if (m_keyRight) m_camera.offsetX += panDelta;

    // Clear to black
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);

    // Lock the snapshot and draw
    {
        std::lock_guard<std::mutex> lock(snapshot.mtx);

        if (snapshot.width == 0 || snapshot.height == 0) {
            // No map data yet — just show black screen
            SDL_RenderPresent(m_renderer);
            return;
        }

        // Auto-fit camera on first frame with valid map data
        if (!m_cameraInitialized) {
            autoFitCamera(snapshot);
            m_cameraInitialized = true;
        }

        float tileW = TILE_SIZE * m_camera.zoom;
        float tileH = TILE_SIZE * m_camera.zoom;

        for (int y = 0; y < snapshot.height; y++) {
            for (int x = 0; x < snapshot.width; x++) {
                const PlotData& plot = snapshot.getPlot(x, y);

                // World position to screen position
                float screenX = x * tileW - m_camera.offsetX * m_camera.zoom;
                float screenY = (snapshot.height - 1 - y) * tileH - m_camera.offsetY * m_camera.zoom;

                // Frustum cull — skip tiles outside the window
                if (screenX + tileW < 0 || screenX > m_windowW ||
                    screenY + tileH < 0 || screenY > m_windowH)
                    continue;

                // Get terrain color
                TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);

                SDL_Rect rect;
                rect.x = (int)screenX;
                rect.y = (int)screenY;
                rect.w = std::max(1, (int)tileW);
                rect.h = std::max(1, (int)tileH);

                SDL_SetRenderDrawColor(m_renderer, tc.r, tc.g, tc.b, 255);
                SDL_RenderFillRect(m_renderer, &rect);

                // Draw city marker (white filled circle approximation — small square)
                if (plot.isCity && tileW >= 4) {
                    int markerSize = std::max(2, (int)(tileW * 0.4f));
                    SDL_Rect marker;
                    marker.x = rect.x + (rect.w - markerSize) / 2;
                    marker.y = rect.y + (rect.h - markerSize) / 2;
                    marker.w = markerSize;
                    marker.h = markerSize;
                    SDL_SetRenderDrawColor(m_renderer, plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, 255);
                    SDL_RenderFillRect(m_renderer, &marker);
                }

                // Draw territory border (colored outline where owner differs from neighbor)
                if (plot.ownerID >= 0 && tileW >= 3) {
                    // Check right neighbor
                    if (x + 1 < snapshot.width) {
                        const PlotData& right = snapshot.getPlot(x + 1, y);
                        if (right.ownerID != plot.ownerID) {
                            SDL_SetRenderDrawColor(m_renderer, plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, 255);
                            SDL_RenderDrawLine(m_renderer, rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h);
                        }
                    }
                    // Check top neighbor
                    if (y + 1 < snapshot.height) {
                        const PlotData& top = snapshot.getPlot(x, y + 1);
                        if (top.ownerID != plot.ownerID) {
                            SDL_SetRenderDrawColor(m_renderer, plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, 255);
                            SDL_RenderDrawLine(m_renderer, rect.x, rect.y, rect.x + rect.w, rect.y);
                        }
                    }
                }

                // Draw river indicator (blue tint on south/west edges)
                if (plot.isRiver && tileW >= 4) {
                    SDL_SetRenderDrawColor(m_renderer, 50, 100, 200, 255);
                    // Draw a line at bottom edge
                    SDL_RenderDrawLine(m_renderer, rect.x, rect.y + rect.h - 1,
                                       rect.x + rect.w, rect.y + rect.h - 1);
                }
            }
        }

        // Draw HUD overlay
        drawHUD(snapshot);
    }

    SDL_RenderPresent(m_renderer);
}

void Renderer::drawHUD(MapSnapshot& snapshot)
{
    // Simple HUD: draw a turn counter in top-left using rectangles
    // (Full text rendering comes in Milestone 5 with SDL2_ttf)

    // Draw a dark background bar at the top
    SDL_Rect hudBg = {0, 0, 200, 24};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(m_renderer, &hudBg);

    // We can't render text without SDL_ttf yet, so just draw colored
    // indicators: a series of small squares showing the turn count
    // in unary (one dot per 10 turns). This is temporary.
    int dots = snapshot.gameTurn / 10;
    for (int i = 0; i < std::min(dots, 20); i++) {
        SDL_Rect dot = {4 + i * 9, 8, 7, 7};
        SDL_SetRenderDrawColor(m_renderer, 0, 200, 0, 255);
        SDL_RenderFillRect(m_renderer, &dot);
    }
}

void Renderer::handleKeyDown(SDL_Keycode key, MapSnapshot& snapshot)
{
    switch (key) {
        case SDLK_UP:    case SDLK_w: m_keyUp = true;    break;
        case SDLK_DOWN:  case SDLK_s: m_keyDown = true;  break;
        case SDLK_LEFT:  case SDLK_a: m_keyLeft = true;  break;
        case SDLK_RIGHT: case SDLK_d: m_keyRight = true; break;
        case SDLK_f: case SDLK_HOME:
            // Fit map to window
            {
                std::lock_guard<std::mutex> lock(snapshot.mtx);
                if (snapshot.width > 0 && snapshot.height > 0)
                    autoFitCamera(snapshot);
            }
            break;
    }
}

void Renderer::handleKeyUp(SDL_Keycode key)
{
    switch (key) {
        case SDLK_UP:    case SDLK_w: m_keyUp = false;    break;
        case SDLK_DOWN:  case SDLK_s: m_keyDown = false;  break;
        case SDLK_LEFT:  case SDLK_a: m_keyLeft = false;  break;
        case SDLK_RIGHT: case SDLK_d: m_keyRight = false; break;
    }
}

void Renderer::handleMouseWheel(int y, int mouseX, int mouseY)
{
    // Zoom toward/away from mouse cursor
    float oldZoom = m_camera.zoom;

    if (y > 0)
        m_camera.zoom *= 1.15f;  // zoom in
    else if (y < 0)
        m_camera.zoom /= 1.15f;  // zoom out

    // Clamp zoom
    m_camera.zoom = std::max(0.1f, std::min(m_camera.zoom, 20.0f));

    // Adjust offset so the point under the cursor stays fixed
    float zoomRatio = m_camera.zoom / oldZoom;
    float worldMouseX = mouseX / oldZoom + m_camera.offsetX;
    float worldMouseY = mouseY / oldZoom + m_camera.offsetY;
    m_camera.offsetX = worldMouseX - mouseX / m_camera.zoom;
    m_camera.offsetY = worldMouseY - mouseY / m_camera.zoom;
}

void Renderer::handleMouseMotion(int dx, int dy, bool anyDragButton)
{
    if (anyDragButton) {
        m_camera.offsetX -= dx / m_camera.zoom;
        m_camera.offsetY -= dy / m_camera.zoom;
    }
}

void Renderer::handleResize(int newW, int newH)
{
    m_windowW = newW;
    m_windowH = newH;
}
