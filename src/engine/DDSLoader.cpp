// DDSLoader.cpp — DDS texture decompression (DXT1/DXT3/DXT5 + uncompressed)
//
// DDS format reference: Microsoft DDS Programming Guide
// DXT block compression: each 4x4 pixel block is compressed independently.

#include "DDSLoader.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

// DDS header constants
static constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "
static constexpr uint32_t DDPF_FOURCC      = 0x00000004;
static constexpr uint32_t DDPF_RGB         = 0x00000040;
static constexpr uint32_t DDPF_ALPHAPIXELS = 0x00000001;

static uint32_t makeFourCC(char a, char b, char c, char d) {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}

// Decode a 16-bit 5:6:5 color to R,G,B (0-255)
static void decodeColor565(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
    g = (uint8_t)(((c >> 5)  & 0x3F) * 255 / 63);
    b = (uint8_t)(( c        & 0x1F) * 255 / 31);
}

// Decode a DXT color block (shared by DXT1/DXT3/DXT5)
// Writes 4x4 pixels (RGB only, alpha set to 255) into outBlock[16*4]
// outBlock layout: 16 pixels, each 4 bytes (RGBA), row-major within the 4x4 block
static void decodeDXTColorBlock(const uint8_t* blockData, uint8_t outBlock[64], bool dxt1Alpha) {
    uint16_t color0 = blockData[0] | (blockData[1] << 8);
    uint16_t color1 = blockData[2] | (blockData[3] << 8);
    uint32_t indices = blockData[4] | (blockData[5] << 8) | (blockData[6] << 16) | (blockData[7] << 24);

    uint8_t r0, g0, b0, r1, g1, b1;
    decodeColor565(color0, r0, g0, b0);
    decodeColor565(color1, r1, g1, b1);

    uint8_t colors[4][4]; // [index][RGBA]
    colors[0][0] = r0; colors[0][1] = g0; colors[0][2] = b0; colors[0][3] = 255;
    colors[1][0] = r1; colors[1][1] = g1; colors[1][2] = b1; colors[1][3] = 255;

    if (color0 > color1) {
        // 4-color mode
        colors[2][0] = (uint8_t)((2 * r0 + r1) / 3);
        colors[2][1] = (uint8_t)((2 * g0 + g1) / 3);
        colors[2][2] = (uint8_t)((2 * b0 + b1) / 3);
        colors[2][3] = 255;
        colors[3][0] = (uint8_t)((r0 + 2 * r1) / 3);
        colors[3][1] = (uint8_t)((g0 + 2 * g1) / 3);
        colors[3][2] = (uint8_t)((b0 + 2 * b1) / 3);
        colors[3][3] = 255;
    } else {
        // 3-color + transparent mode (DXT1 only)
        colors[2][0] = (uint8_t)((r0 + r1) / 2);
        colors[2][1] = (uint8_t)((g0 + g1) / 2);
        colors[2][2] = (uint8_t)((b0 + b1) / 2);
        colors[2][3] = 255;
        colors[3][0] = 0;
        colors[3][1] = 0;
        colors[3][2] = 0;
        colors[3][3] = dxt1Alpha ? 0 : 255;
    }

    for (int i = 0; i < 16; i++) {
        int idx = (indices >> (i * 2)) & 0x3;
        outBlock[i * 4 + 0] = colors[idx][0];
        outBlock[i * 4 + 1] = colors[idx][1];
        outBlock[i * 4 + 2] = colors[idx][2];
        outBlock[i * 4 + 3] = colors[idx][3];
    }
}

// Decompress DXT1 (8 bytes per 4x4 block, optional 1-bit alpha)
static bool decompressDXT1(const uint8_t* src, size_t srcSize,
                           uint32_t width, uint32_t height, std::vector<uint8_t>& pixels) {
    uint32_t bw = (width + 3) / 4;
    uint32_t bh = (height + 3) / 4;
    size_t needed = (size_t)bw * bh * 8;
    if (srcSize < needed) return false;

    pixels.resize(width * height * 4);

    for (uint32_t by = 0; by < bh; by++) {
        for (uint32_t bx = 0; bx < bw; bx++) {
            uint8_t block[64]; // 16 pixels * 4 bytes
            decodeDXTColorBlock(src, block, true);
            src += 8;

            // Copy block pixels into output image
            for (int py = 0; py < 4; py++) {
                uint32_t iy = by * 4 + py;
                if (iy >= height) continue;
                for (int px = 0; px < 4; px++) {
                    uint32_t ix = bx * 4 + px;
                    if (ix >= width) continue;
                    size_t dstOff = (iy * width + ix) * 4;
                    size_t srcOff = (py * 4 + px) * 4;
                    pixels[dstOff + 0] = block[srcOff + 0];
                    pixels[dstOff + 1] = block[srcOff + 1];
                    pixels[dstOff + 2] = block[srcOff + 2];
                    pixels[dstOff + 3] = block[srcOff + 3];
                }
            }
        }
    }
    return true;
}

// Decompress DXT3 (16 bytes per 4x4 block: 8 bytes alpha + 8 bytes color)
static bool decompressDXT3(const uint8_t* src, size_t srcSize,
                           uint32_t width, uint32_t height, std::vector<uint8_t>& pixels) {
    uint32_t bw = (width + 3) / 4;
    uint32_t bh = (height + 3) / 4;
    size_t needed = (size_t)bw * bh * 16;
    if (srcSize < needed) return false;

    pixels.resize(width * height * 4);

    for (uint32_t by = 0; by < bh; by++) {
        for (uint32_t bx = 0; bx < bw; bx++) {
            // First 8 bytes: explicit 4-bit alpha for each of 16 pixels
            const uint8_t* alphaBlock = src;
            src += 8;

            // Next 8 bytes: DXT color block
            uint8_t block[64];
            decodeDXTColorBlock(src, block, false);
            src += 8;

            // Apply explicit alpha: 2 pixels per byte (4 bits each)
            for (int i = 0; i < 16; i++) {
                int byteIdx = i / 2;
                int nibble = (i & 1) ? (alphaBlock[byteIdx] >> 4) : (alphaBlock[byteIdx] & 0x0F);
                block[i * 4 + 3] = (uint8_t)(nibble * 17); // 0-15 -> 0-255
            }

            // Copy to output
            for (int py = 0; py < 4; py++) {
                uint32_t iy = by * 4 + py;
                if (iy >= height) continue;
                for (int px = 0; px < 4; px++) {
                    uint32_t ix = bx * 4 + px;
                    if (ix >= width) continue;
                    size_t dstOff = (iy * width + ix) * 4;
                    size_t srcOff = (py * 4 + px) * 4;
                    pixels[dstOff + 0] = block[srcOff + 0];
                    pixels[dstOff + 1] = block[srcOff + 1];
                    pixels[dstOff + 2] = block[srcOff + 2];
                    pixels[dstOff + 3] = block[srcOff + 3];
                }
            }
        }
    }
    return true;
}

// Decompress DXT5 (16 bytes per 4x4 block: 8 bytes interpolated alpha + 8 bytes color)
static bool decompressDXT5(const uint8_t* src, size_t srcSize,
                           uint32_t width, uint32_t height, std::vector<uint8_t>& pixels) {
    uint32_t bw = (width + 3) / 4;
    uint32_t bh = (height + 3) / 4;
    size_t needed = (size_t)bw * bh * 16;
    if (srcSize < needed) return false;

    pixels.resize(width * height * 4);

    for (uint32_t by = 0; by < bh; by++) {
        for (uint32_t bx = 0; bx < bw; bx++) {
            // Alpha block: 2 reference alphas + 48 bits of 3-bit indices
            uint8_t alpha0 = src[0];
            uint8_t alpha1 = src[1];
            // 6 bytes of packed 3-bit indices (16 pixels * 3 bits = 48 bits)
            uint64_t alphaBits = 0;
            for (int i = 0; i < 6; i++)
                alphaBits |= (uint64_t)src[2 + i] << (8 * i);
            src += 8;

            // Build alpha lookup table
            uint8_t alphas[8];
            alphas[0] = alpha0;
            alphas[1] = alpha1;
            if (alpha0 > alpha1) {
                for (int i = 0; i < 6; i++)
                    alphas[2 + i] = (uint8_t)(((6 - i) * alpha0 + (1 + i) * alpha1) / 7);
            } else {
                for (int i = 0; i < 4; i++)
                    alphas[2 + i] = (uint8_t)(((4 - i) * alpha0 + (1 + i) * alpha1) / 5);
                alphas[6] = 0;
                alphas[7] = 255;
            }

            // Color block
            uint8_t block[64];
            decodeDXTColorBlock(src, block, false);
            src += 8;

            // Apply alpha
            for (int i = 0; i < 16; i++) {
                int alphaIdx = (int)((alphaBits >> (3 * i)) & 0x7);
                block[i * 4 + 3] = alphas[alphaIdx];
            }

            // Copy to output
            for (int py = 0; py < 4; py++) {
                uint32_t iy = by * 4 + py;
                if (iy >= height) continue;
                for (int px = 0; px < 4; px++) {
                    uint32_t ix = bx * 4 + px;
                    if (ix >= width) continue;
                    size_t dstOff = (iy * width + ix) * 4;
                    size_t srcOff = (py * 4 + px) * 4;
                    pixels[dstOff + 0] = block[srcOff + 0];
                    pixels[dstOff + 1] = block[srcOff + 1];
                    pixels[dstOff + 2] = block[srcOff + 2];
                    pixels[dstOff + 3] = block[srcOff + 3];
                }
            }
        }
    }
    return true;
}

bool loadDDS(const uint8_t* data, size_t dataSize, DDSImage& out) {
    if (dataSize < 128) {
        fprintf(stderr, "DDS: file too small (%zu bytes)\n", dataSize);
        return false;
    }

    // Verify magic
    uint32_t magic;
    memcpy(&magic, data, 4);
    if (magic != DDS_MAGIC) {
        fprintf(stderr, "DDS: bad magic\n");
        return false;
    }

    // Parse header (starts at offset 4, 124 bytes)
    const uint8_t* hdr = data + 4;
    uint32_t hdrSize;  memcpy(&hdrSize, hdr + 0, 4);
    uint32_t flags;    memcpy(&flags,   hdr + 4, 4);
    uint32_t height;   memcpy(&height,  hdr + 8, 4);
    uint32_t width;    memcpy(&width,   hdr + 12, 4);

    // Pixel format at hdr+72
    const uint8_t* pf = hdr + 72;
    uint32_t pfFlags;  memcpy(&pfFlags, pf + 4, 4);
    uint32_t fourCC;   memcpy(&fourCC,  pf + 8, 4);
    uint32_t rgbBits;  memcpy(&rgbBits, pf + 12, 4);
    uint32_t rMask;    memcpy(&rMask,   pf + 16, 4);
    uint32_t gMask;    memcpy(&gMask,   pf + 20, 4);
    uint32_t bMask;    memcpy(&bMask,   pf + 24, 4);
    uint32_t aMask;    memcpy(&aMask,   pf + 28, 4);

    out.width = width;
    out.height = height;

    const uint8_t* pixelData = data + 128; // pixel data starts after 128-byte header
    size_t pixelDataSize = dataSize - 128;

    if (pfFlags & DDPF_FOURCC) {
        // Compressed format
        if (fourCC == makeFourCC('D','X','T','1')) {
            return decompressDXT1(pixelData, pixelDataSize, width, height, out.pixels);
        } else if (fourCC == makeFourCC('D','X','T','3')) {
            return decompressDXT3(pixelData, pixelDataSize, width, height, out.pixels);
        } else if (fourCC == makeFourCC('D','X','T','5')) {
            return decompressDXT5(pixelData, pixelDataSize, width, height, out.pixels);
        } else {
            fprintf(stderr, "DDS: unsupported FourCC 0x%08X\n", fourCC);
            return false;
        }
    } else if (pfFlags & DDPF_RGB) {
        // Uncompressed RGB(A)
        uint32_t bytesPerPixel = rgbBits / 8;
        size_t needed = (size_t)width * height * bytesPerPixel;
        if (pixelDataSize < needed) {
            fprintf(stderr, "DDS: truncated uncompressed data\n");
            return false;
        }

        out.pixels.resize(width * height * 4);
        bool hasAlpha = (pfFlags & DDPF_ALPHAPIXELS) != 0;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                size_t srcOff = (y * width + x) * bytesPerPixel;
                size_t dstOff = (y * width + x) * 4;

                uint32_t pixel = 0;
                memcpy(&pixel, pixelData + srcOff, bytesPerPixel);

                // Extract channels using masks
                auto extractChannel = [](uint32_t pixel, uint32_t mask) -> uint8_t {
                    if (mask == 0) return 255;
                    int shift = 0;
                    uint32_t m = mask;
                    while ((m & 1) == 0) { m >>= 1; shift++; }
                    uint32_t val = (pixel & mask) >> shift;
                    // Scale to 0-255 based on mask width
                    int bits = 0;
                    while (m) { bits++; m >>= 1; }
                    return (uint8_t)(val * 255 / ((1 << bits) - 1));
                };

                out.pixels[dstOff + 0] = extractChannel(pixel, rMask);
                out.pixels[dstOff + 1] = extractChannel(pixel, gMask);
                out.pixels[dstOff + 2] = extractChannel(pixel, bMask);
                out.pixels[dstOff + 3] = hasAlpha ? extractChannel(pixel, aMask) : 255;
            }
        }
        return true;
    } else {
        fprintf(stderr, "DDS: unsupported pixel format flags 0x%08X\n", pfFlags);
        return false;
    }
}
