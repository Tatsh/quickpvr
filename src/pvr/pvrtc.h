#pragma once

#include <cstdint>

/** One 64-bit PVRTC block as two 32-bit words. */
struct AMTC_BLOCK_STRUCT {
    std::uint32_t PackedData[2]; /*!< Modulation data, then the two colours. */
};

/**
 * Decompress PVRTC 1 data to 8-bit RGBA.
 *
 * @param pCompressedData The blocks, at least the size PVRTC requires for the dimensions.
 * @param Do2bitMode Non-zero for 2 bpp data, zero for 4 bpp.
 * @param XDim Width in pixels, a power of two.
 * @param YDim Height in pixels, a power of two.
 * @param AssumeImageTiles Non-zero to wrap at the edges, as the hardware does.
 * @param pResultImage Receives `XDim * YDim * 4` bytes.
 */
void Decompress(AMTC_BLOCK_STRUCT *pCompressedData,
                int Do2bitMode,
                int XDim,
                int YDim,
                int AssumeImageTiles,
                unsigned char *pResultImage);
