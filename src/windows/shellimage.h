#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include <windows.h>

#include <objidl.h>

#include "pvr/pvr.h"

namespace QuickPvrShell {

/** A decoded texture, or a pack placed on a contact sheet, with a description of it. */
struct DecodedTexture {
    Pvr::Image image;     /*!< The texture or contact sheet. */
    std::wstring summary; /*!< Description such as `Dreamcast PVR · 256×256 RGB565, VQ`. */
};

/**
 * Read a whole stream.
 *
 * @param stream The stream, read from its current position.
 * @return The bytes, or `E_OUTOFMEMORY` when the stream exceeds the size limit, or the stream
 * error.
 */
std::expected<std::vector<std::uint8_t>, HRESULT> readStream(IStream *stream);

/**
 * Decode texture data.
 *
 * @param data The file contents.
 * @return The texture, or `E_FAIL` when the data cannot be decoded.
 */
std::expected<DecodedTexture, HRESULT> decodeTexture(const std::vector<std::uint8_t> &data);

/**
 * Scale an image to fit inside a box, preserving its aspect ratio. Enlarging repeats texels so they
 * remain sharp; shrinking averages them.
 *
 * @param image The image.
 * @param maxWidth Width of the box.
 * @param maxHeight Height of the box.
 * @return The scaled image.
 */
Pvr::Image scaleToFit(const Pvr::Image &image, std::uint32_t maxWidth, std::uint32_t maxHeight);

/**
 * Create a 32-bit top-down DIB section with straight-alpha BGRA pixels.
 *
 * @param image The image.
 * @return The bitmap, or `nullptr` on failure. The caller is responsible for deleting it.
 */
HBITMAP createBitmap(const Pvr::Image &image);

} // namespace QuickPvrShell
