// Renderer.cpp — 2D map renderer with hex grid
//
// Milestone 3: Flat-top hexagonal grid with terrain colors,
// camera controls, territory borders, city markers, rivers.

#include "Renderer.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

// Precomputed cos/sin for flat-top hex vertices at 0°,60°,120°,180°,240°,300°
static const float HEX_COS[6] = { 1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f };
static const float HEX_SIN[6] = { 0.0f, 0.866025404f, 0.866025404f, 0.0f, -0.866025404f, -0.866025404f };

Renderer::Renderer(SDL_Renderer* sdlRenderer, int windowW, int windowH)
    : m_renderer(sdlRenderer)
    , m_windowW(windowW)
    , m_windowH(windowH)
{
}

// ---------- Hex geometry helpers ----------

void Renderer::hexCenter(int col, int row, int mapHeight, float& cx, float& cy) const
{
    // Flat-top hex layout:
    //   Column spacing = 1.5 * R (columns interlock)
    //   Row spacing = sqrt(3) * R
    //   Odd columns shift down by half a row
    cx = col * COL_SPACING + HEX_RADIUS;

    // Flip Y: game row 0 is at bottom, screen Y=0 is at top
    int flippedRow = mapHeight - 1 - row;
    cy = flippedRow * ROW_SPACING + HEX_HEIGHT / 2.0f;

    // Odd columns shift down by half a row
    if (col % 2 != 0)
        cy += ROW_SPACING / 2.0f;
}

void Renderer::drawFilledHex(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b)
{
    // 7 vertices: center + 6 corners
    SDL_Vertex verts[7];

    // Center vertex
    verts[0].position.x = cx;
    verts[0].position.y = cy;
    verts[0].color.r = r;
    verts[0].color.g = g;
    verts[0].color.b = b;
    verts[0].color.a = 255;
    verts[0].tex_coord.x = 0;
    verts[0].tex_coord.y = 0;

    // 6 corner vertices
    for (int i = 0; i < 6; i++) {
        verts[i + 1].position.x = cx + radius * HEX_COS[i];
        verts[i + 1].position.y = cy + radius * HEX_SIN[i];
        verts[i + 1].color = verts[0].color;
        verts[i + 1].tex_coord.x = 0;
        verts[i + 1].tex_coord.y = 0;
    }

    // 6 triangles: center(0) → corner[i] → corner[i+1]
    int indices[18];
    for (int i = 0; i < 6; i++) {
        indices[i * 3]     = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % 6 + 1;
    }

    SDL_RenderGeometry(m_renderer, nullptr, verts, 7, indices, 18);
}

void Renderer::drawHexOutline(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b)
{
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);

    for (int i = 0; i < 6; i++) {
        int j = (i + 1) % 6;
        SDL_RenderDrawLine(m_renderer,
            (int)(cx + radius * HEX_COS[i]), (int)(cy + radius * HEX_SIN[i]),
            (int)(cx + radius * HEX_COS[j]), (int)(cy + radius * HEX_SIN[j]));
    }
}

// ---------- Camera ----------

void Renderer::autoFitCamera(const MapSnapshot& snapshot)
{
    // Total world-space dimensions of the hex map
    float mapPixelW = (snapshot.width - 1) * COL_SPACING + HEX_WIDTH;
    float mapPixelH = (snapshot.height - 1) * ROW_SPACING + HEX_HEIGHT + ROW_SPACING / 2.0f;

    float zoomX = (m_windowW * 0.9f) / mapPixelW;
    float zoomY = (m_windowH * 0.9f) / mapPixelH;
    m_camera.zoom = std::min(zoomX, zoomY);
    m_camera.zoom = std::max(0.1f, std::min(m_camera.zoom, 20.0f));

    // Center the map in the window
    float scaledW = mapPixelW * m_camera.zoom;
    float scaledH = mapPixelH * m_camera.zoom;
    m_camera.offsetX = -(m_windowW - scaledW) / (2.0f * m_camera.zoom);
    m_camera.offsetY = -(m_windowH - scaledH) / (2.0f * m_camera.zoom);

    fprintf(stderr, "[renderer] Auto-fit hex: map=%dx%d zoom=%.2f offset=(%.0f,%.0f)\n",
            snapshot.width, snapshot.height, m_camera.zoom,
            m_camera.offsetX, m_camera.offsetY);
}

// ---------- Main draw ----------

void Renderer::draw(MapSnapshot& snapshot)
{
    // Update camera position from held keys
    float dt = 1.0f / 60.0f;
    float panDelta = m_panSpeed * dt / m_camera.zoom;
    if (m_keyUp)    m_camera.offsetY -= panDelta;
    if (m_keyDown)  m_camera.offsetY += panDelta;
    if (m_keyLeft)  m_camera.offsetX -= panDelta;
    if (m_keyRight) m_camera.offsetX += panDelta;

    // Clear to dark ocean background
    SDL_SetRenderDrawColor(m_renderer, 10, 20, 40, 255);
    SDL_RenderClear(m_renderer);

    // Lock the snapshot and draw
    {
        std::lock_guard<std::mutex> lock(snapshot.mtx);

        if (snapshot.width == 0 || snapshot.height == 0) {
            SDL_RenderPresent(m_renderer);
            return;
        }

        // Auto-fit camera on first frame with valid map data
        if (!m_cameraInitialized) {
            autoFitCamera(snapshot);
            m_cameraInitialized = true;
        }

        float screenRadius = HEX_RADIUS * m_camera.zoom;

        for (int y = 0; y < snapshot.height; y++) {
            for (int x = 0; x < snapshot.width; x++) {
                const PlotData& plot = snapshot.getPlot(x, y);

                // Get world-space hex center
                float worldCX, worldCY;
                hexCenter(x, y, snapshot.height, worldCX, worldCY);

                // Transform to screen space
                float screenCX = (worldCX - m_camera.offsetX) * m_camera.zoom;
                float screenCY = (worldCY - m_camera.offsetY) * m_camera.zoom;

                // Frustum cull — skip hexes entirely outside the window
                if (screenCX + screenRadius < 0 || screenCX - screenRadius > m_windowW ||
                    screenCY + screenRadius < 0 || screenCY - screenRadius > m_windowH)
                    continue;

                // Draw filled hex with terrain color
                TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);
                drawFilledHex(screenCX, screenCY, screenRadius, tc.r, tc.g, tc.b);

                // City marker (smaller hex in owner's color)
                if (plot.isCity && screenRadius >= 2) {
                    drawFilledHex(screenCX, screenCY, screenRadius * 0.35f,
                                  plot.ownerColorR, plot.ownerColorG, plot.ownerColorB);
                }

                // Territory border: outline owned hexes that border a different owner
                if (plot.ownerID >= 0 && screenRadius >= 2) {
                    bool needBorder = false;

                    // Check 4 cardinal neighbors for ownership change
                    if (x + 1 < snapshot.width && snapshot.getPlot(x + 1, y).ownerID != plot.ownerID)
                        needBorder = true;
                    if (x - 1 >= 0 && snapshot.getPlot(x - 1, y).ownerID != plot.ownerID)
                        needBorder = true;
                    if (y + 1 < snapshot.height && snapshot.getPlot(x, y + 1).ownerID != plot.ownerID)
                        needBorder = true;
                    if (y - 1 >= 0 && snapshot.getPlot(x, y - 1).ownerID != plot.ownerID)
                        needBorder = true;

                    // Also draw border at map edges
                    if (x == 0 || x == snapshot.width - 1 || y == 0 || y == snapshot.height - 1)
                        needBorder = true;

                    if (needBorder) {
                        drawHexOutline(screenCX, screenCY, screenRadius,
                                       plot.ownerColorR, plot.ownerColorG, plot.ownerColorB);
                    }
                }

                // River indicator (blue hex outline, slightly inset)
                if (plot.isRiver && screenRadius >= 3) {
                    drawHexOutline(screenCX, screenCY, screenRadius * 0.85f, 50, 100, 200);
                }
            }
        }

        // Draw HUD overlay
        drawHUD(snapshot);
    }

    SDL_RenderPresent(m_renderer);
}

// ---------- HUD ----------

void Renderer::drawHUD(MapSnapshot& snapshot)
{
    // Dark background bar at the top
    SDL_Rect hudBg = {0, 0, 200, 24};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(m_renderer, &hudBg);

    // Turn counter: one green dot per 10 turns (placeholder until SDL_ttf)
    int dots = snapshot.gameTurn / 10;
    for (int i = 0; i < std::min(dots, 20); i++) {
        SDL_Rect dot = {4 + i * 9, 8, 7, 7};
        SDL_SetRenderDrawColor(m_renderer, 0, 200, 0, 255);
        SDL_RenderFillRect(m_renderer, &dot);
    }
}

// ---------- Input handling ----------

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
