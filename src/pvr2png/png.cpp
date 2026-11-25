#include "png.h"

#include <climits>
#include <cstddef>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

namespace Pvr2Png {

namespace {

constexpr int kChannelCount = 4;

void appendBytes(void *context, void *data, int size) {
    auto &out = *static_cast<std::vector<std::uint8_t> *>(context);
    const auto *bytes = static_cast<const std::uint8_t *>(data);
    out.insert(out.end(), bytes, bytes + size);
}

} // namespace

std::expected<std::vector<std::uint8_t>, std::string> encodePng(const Pvr::Image &image) {
    if (image.width > INT_MAX / kChannelCount || image.height > INT_MAX) {
        return std::unexpected("The image is too large to encode.");
    }
    std::vector<std::uint8_t> out;
    const auto width = static_cast<int>(image.width);
    const auto written = stbi_write_png_to_func(appendBytes,
                                                &out,
                                                width,
                                                static_cast<int>(image.height),
                                                kChannelCount,
                                                image.rgba.data(),
                                                width * kChannelCount);
    if (written == 0) {
        return std::unexpected("stb_image_write could not encode the image.");
    }
    return out;
}

} // namespace Pvr2Png
