#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "pvr/pvr.h"

namespace Pvr2Png {

/**
 * Encode an image as an 8-bit RGBA PNG.
 *
 * @param image The image.
 * @return The PNG file contents, or a message when compression fails.
 */
std::expected<std::vector<std::uint8_t>, std::string> encodePng(const Pvr::Image &image);

} // namespace Pvr2Png
