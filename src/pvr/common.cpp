#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#include "internal.h"

#ifndef PVR_VERSION
#define PVR_VERSION "0.0.0"
#endif

namespace Pvr {

namespace Detail {

namespace {

constexpr unsigned kBitsPerByte = 8;
constexpr unsigned kChannelCount = 4;
constexpr unsigned kAlphaChannel = 3;
constexpr unsigned kOpaque = 255;
constexpr int kChromaZero = 128;
constexpr double kRedFromRedDifference = 1.402;
constexpr double kGreenFromBlueDifference = 0.344136;
constexpr double kGreenFromRedDifference = 0.714136;
constexpr double kBlueFromBlueDifference = 1.772;
constexpr unsigned kMortonBits = 16;

std::size_t morton(std::uint32_t x, std::uint32_t y) {
    std::size_t index = 0;
    for (unsigned bit = 0; bit < kMortonBits; ++bit) {
        index |= static_cast<std::size_t>((y >> bit) & 1) << (2 * bit);
        index |= static_cast<std::size_t>((x >> bit) & 1) << (2 * bit + 1);
    }
    return index;
}

std::uint8_t clampToByte(double value) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

} // namespace

std::unexpected<Error> fail(ErrorCode code, std::string message) {
    return std::unexpected(Error{code, std::move(message)});
}

std::string hex(unsigned value) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "0x%02x", value);
    return buffer;
}

std::expected<Bytes, Error>
slice(Bytes data, std::size_t offset, std::size_t length, std::string_view what) {
    if (!hasRange(data, offset, length)) {
        return fail(ErrorCode::Truncated,
                    std::string(what) + " needs " + std::to_string(length) + " bytes at offset " +
                        std::to_string(offset) + " but the data is " + std::to_string(data.size()) +
                        " bytes long.");
    }
    return data.subspan(offset, length);
}

std::expected<Image, Error>
makeImage(std::uint32_t width, std::uint32_t height, std::string format, const Options &options) {
    const auto size = std::to_string(width) + "x" + std::to_string(height);
    if (width == 0 || height == 0) {
        return fail(ErrorCode::Invalid, "The texture has a zero dimension (" + size + ").");
    }
    if (width > options.maxDimension || height > options.maxDimension ||
        static_cast<std::uint64_t>(width) * height > options.maxPixels) {
        return fail(ErrorCode::TooLarge, "The texture is too large (" + size + ").");
    }
    Image image;
    image.width = width;
    image.height = height;
    image.format = std::move(format);
    image.rgba.assign(static_cast<std::size_t>(width) * height * kChannelCount, 0);
    return image;
}

std::uint8_t expandBits(std::uint64_t value, unsigned bits) {
    if (bits == 0) {
        return kOpaque;
    }
    if (bits >= kBitsPerByte) {
        return static_cast<std::uint8_t>(value >> (bits - kBitsPerByte));
    }
    unsigned result = 0;
    auto shift = static_cast<int>(kBitsPerByte - bits);
    while (shift > -static_cast<int>(bits)) {
        result |= shift >= 0 ? static_cast<unsigned>(value << shift) :
                               static_cast<unsigned>(value >> -shift);
        shift -= static_cast<int>(bits);
    }
    return static_cast<std::uint8_t>(result);
}

std::uint8_t maskedChannel(std::uint64_t pixel, std::uint64_t mask) {
    if (mask == 0) {
        return kOpaque;
    }
    const auto shift = static_cast<unsigned>(std::countr_zero(mask));
    return expandBits((pixel & mask) >> shift, static_cast<unsigned>(std::popcount(mask)));
}

std::size_t
twiddledIndex(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height) {
    const auto side = std::min(width, height);
    const auto blockArea = static_cast<std::size_t>(side) * side;
    const auto block = width >= height ? x / side : y / side;
    return block * blockArea + morton(x % side, y % side);
}

void flipVertical(Image &image) {
    const auto row = static_cast<std::size_t>(image.width) * kChannelCount;
    for (std::uint32_t y = 0; y < image.height / 2; ++y) {
        auto *top = image.rgba.data() + y * row;
        auto *bottom = image.rgba.data() + (image.height - 1 - y) * row;
        std::swap_ranges(top, top + row, bottom);
    }
}

void flipHorizontal(Image &image) {
    for (std::uint32_t y = 0; y < image.height; ++y) {
        auto *row = image.rgba.data() + static_cast<std::size_t>(y) * image.width * kChannelCount;
        for (std::uint32_t x = 0; x < image.width / 2; ++x) {
            auto *left = row + x * kChannelCount;
            auto *right = row + (image.width - 1 - x) * kChannelCount;
            std::swap_ranges(left, left + kChannelCount, right);
        }
    }
}

void unpremultiply(Image &image) {
    for (std::size_t i = 0; i < image.rgba.size(); i += kChannelCount) {
        const unsigned alpha = image.rgba[i + kAlphaChannel];
        if (alpha == 0 || alpha == kOpaque) {
            continue;
        }
        for (std::size_t c = 0; c < kAlphaChannel; ++c) {
            const auto straight = (image.rgba[i + c] * kOpaque + alpha / 2) / alpha;
            image.rgba[i + c] = static_cast<std::uint8_t>(std::min(kOpaque, straight));
        }
    }
}

Color yuvToRgb(int luma, int blueDifference, int redDifference) {
    const double cb = blueDifference - kChromaZero;
    const double cr = redDifference - kChromaZero;
    return {clampToByte(luma + kRedFromRedDifference * cr),
            clampToByte(luma - kGreenFromBlueDifference * cb - kGreenFromRedDifference * cr),
            clampToByte(luma + kBlueFromBlueDifference * cb),
            static_cast<std::uint8_t>(kOpaque)};
}

std::expected<void, Error> decodeYuv422(Bytes data, bool uyvy, Image &image) {
    constexpr std::size_t kBytesPerPair = 4;
    const auto pairs = (static_cast<std::size_t>(image.width) + 1) / 2;
    const auto body = slice(data, 0, pairs * kBytesPerPair * image.height, "YUV 4:2:2 data");
    if (!body) {
        return std::unexpected(body.error());
    }
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::size_t pair = 0; pair < pairs; ++pair) {
            const auto *p = body->data() + (y * pairs + pair) * kBytesPerPair;
            const int luma0 = uyvy ? p[1] : p[0];
            const int luma1 = uyvy ? p[3] : p[2];
            const int blueDifference = uyvy ? p[0] : p[1];
            const int redDifference = uyvy ? p[2] : p[3];
            const auto x = static_cast<std::uint32_t>(pair * 2);
            setPixel(image, x, y, yuvToRgb(luma0, blueDifference, redDifference));
            if (x + 1 < image.width) {
                setPixel(image, x + 1, y, yuvToRgb(luma1, blueDifference, redDifference));
            }
        }
    }
    return {};
}

} // namespace Detail

using namespace Detail;

std::optional<Container> detect(std::span<const std::uint8_t> data) {
    if (startsWith(data, 0, "PVR\x03")) {
        return Container::Version3;
    }
    if (looksLikeDreamcast(data)) {
        return Container::Dreamcast;
    }
    if (looksLikeLegacy(data)) {
        return Container::Legacy;
    }
    if (looksLikeDreamcastPack(data)) {
        return Container::DreamcastPack;
    }
    if (looksLikeApplePvrtc(data)) {
        return Container::ApplePvrtc;
    }
    return std::nullopt;
}

std::expected<DecodeResult, Error> decode(std::span<const std::uint8_t> data,
                                          const Options &options) {
    const auto container = detect(data);
    if (!container) {
        return fail(ErrorCode::Unrecognized, "Not a recognized PowerVR or Dreamcast texture.");
    }
    switch (*container) {
    case Container::Version3:
        return decodeVersion3(data, options);
    case Container::Dreamcast:
        return decodeDreamcast(data, options);
    case Container::Legacy:
        return decodeLegacy(data, options);
    case Container::DreamcastPack:
        return decodeDreamcastPack(data, options);
    case Container::ApplePvrtc:
        return decodeApplePvrtc(data, options);
    }
    return fail(ErrorCode::Unrecognized, "Unhandled container.");
}

std::expected<Image, Error>
contactSheet(std::span<const Image> images, std::uint32_t padding, const Options &options) {
    if (images.empty()) {
        return fail(ErrorCode::Invalid, "There are no images to place.");
    }
    if (images.size() == 1) {
        return images.front();
    }
    std::uint32_t cellWidth = 0;
    std::uint32_t cellHeight = 0;
    for (const auto &image : images) {
        cellWidth = std::max(cellWidth, image.width);
        cellHeight = std::max(cellHeight, image.height);
    }
    const auto count = static_cast<std::uint32_t>(images.size());
    const auto columns =
        static_cast<std::uint32_t>(std::ceil(std::sqrt(static_cast<double>(count))));
    const auto rows = (count + columns - 1) / columns;
    const auto width = static_cast<std::uint64_t>(columns) * cellWidth + (columns - 1) * padding;
    const auto height = static_cast<std::uint64_t>(rows) * cellHeight + (rows - 1) * padding;
    if (width > options.maxDimension || height > options.maxDimension) {
        return fail(ErrorCode::TooLarge, "The contact sheet would be too large.");
    }
    auto sheet = makeImage(static_cast<std::uint32_t>(width),
                           static_cast<std::uint32_t>(height),
                           std::to_string(count) + " textures",
                           options);
    if (!sheet) {
        return sheet;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto &image = images[i];
        const auto left = (i % columns) * (cellWidth + padding) + (cellWidth - image.width) / 2;
        const auto top = (i / columns) * (cellHeight + padding) + (cellHeight - image.height) / 2;
        const auto row = static_cast<std::size_t>(image.width) * kChannelCount;
        for (std::uint32_t y = 0; y < image.height; ++y) {
            std::copy_n(image.rgba.data() + y * row,
                        row,
                        sheet->rgba.data() +
                            ((static_cast<std::size_t>(top) + y) * sheet->width + left) *
                                kChannelCount);
        }
    }
    return sheet;
}

std::string_view containerName(Container container) {
    switch (container) {
    case Container::Legacy:
        return "PVR v2";
    case Container::Version3:
        return "PVR v3";
    case Container::ApplePvrtc:
        return "Apple PVRTC";
    case Container::Dreamcast:
        return "Dreamcast PVR";
    case Container::DreamcastPack:
        return "Dreamcast PVR pack";
    }
    return "unknown";
}

std::string_view version() {
    return PVR_VERSION;
}

} // namespace Pvr
