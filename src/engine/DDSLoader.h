#pragma once
// DDSLoader — Load DDS textures (DXT1/DXT3/DXT5 + uncompressed) into RGBA pixels
//
// Civ4 terrain textures are primarily DXT3 compressed.
// This loader decompresses them into 32-bit RGBA for use with SDL_Texture.

#include <cstdint>
#include <vector>

struct DDSImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels;  // RGBA, 4 bytes per pixel, row-major top-to-bottom
};

// Load a DDS file from raw bytes (as extracted from FPK or read from disk).
// Returns true on success; fills out width, height, and RGBA pixel data.
// Supports DXT1, DXT3, DXT5, and uncompressed RGBA/BGRA formats.
bool loadDDS(const uint8_t* data, size_t dataSize, DDSImage& out);
