#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "internal.h"

namespace Pvr::Detail {

namespace {

constexpr std::uint32_t kVersion1HeaderSize = 44;
constexpr std::uint32_t kVersion2HeaderSize = 52;
constexpr std::size_t kTagOffset = 44;
constexpr std::size_t kSurfaceCountOffset = 48;
constexpr std::uint32_t kTag = 0x21525650; // "PVR!"
constexpr std::uint32_t kMaxPlausibleDimension = 65536;
constexpr std::uint32_t kMaxPlausibleBitCount = 64;
constexpr unsigned kBitsPerByte = 8;
constexpr std::uint32_t kPixelTypeMask = 0xFF;

constexpr std::uint32_t kFlagMipmaps = 1U << 8;
constexpr std::uint32_t kFlagTwiddled = 1U << 9;
constexpr std::uint32_t kFlagAlpha = 1U << 15;
constexpr std::uint32_t kFlagVerticalFlip = 1U << 16;

// Headerless PVRTC from texturetool is square, power-of-two, and at least an 8x8 4 bpp texture.
constexpr std::size_t kApplePvrtcMinimumSize = 32;
constexpr std::size_t kTwoBitPixelsPerByte = 4;
constexpr std::size_t kFourBitPixelsPerByte = 2;

enum class PixelType : unsigned {
    Argb4444 = 0x00,
    Argb1555 = 0x01,
    Rgb565 = 0x02,
    Rgb555 = 0x03,
    Rgb888 = 0x04,
    Argb8888 = 0x05,
    Argb8332 = 0x06,
    Intensity8 = 0x07,
    AlphaIntensity88 = 0x08,
    Pvrtc2 = 0x0C,
    Pvrtc4 = 0x0D,
    OpenGlRgba4444 = 0x10,
    OpenGlRgba5551 = 0x11,
    OpenGlRgba8888 = 0x12,
    OpenGlRgb565 = 0x13,
    OpenGlRgb555 = 0x14,
    OpenGlRgb888 = 0x15,
    OpenGlIntensity8 = 0x16,
    OpenGlAlphaIntensity88 = 0x17,
    OpenGlPvrtc2 = 0x18,
    OpenGlPvrtc4 = 0x19,
    OpenGlBgra8888 = 0x1A,
    OpenGlAlpha8 = 0x1B,
    Dxt1 = 0x20,
    Dxt2 = 0x21,
    Dxt3 = 0x22,
    Dxt4 = 0x23,
    Dxt5 = 0x24,
    Rgb332 = 0x25,
    AlphaLuminance44 = 0x26,
};

struct Header {
    std::uint32_t headerSize{};
    std::uint32_t height{};
    std::uint32_t width{};
    std::uint32_t mipmapCount{};
    std::uint32_t flags{};
    std::uint32_t dataSize{};
    std::uint32_t bitCount{};
    std::uint64_t redMask{};
    std::uint64_t greenMask{};
    std::uint64_t blueMask{};
    std::uint64_t alphaMask{};
    std::uint32_t surfaceCount{};
};

// The data must include at least a version 1 header.
Header readHeader(Bytes data) {
    Header header;
    header.headerSize = loadU32(data, 0);
    header.height = loadU32(data, 4);
    header.width = loadU32(data, 8);
    header.mipmapCount = loadU32(data, 12);
    header.flags = loadU32(data, 16);
    header.dataSize = loadU32(data, 20);
    header.bitCount = loadU32(data, 24);
    header.redMask = loadU32(data, 28);
    header.greenMask = loadU32(data, 32);
    header.blueMask = loadU32(data, 36);
    header.alphaMask = loadU32(data, 40);
    header.surfaceCount =
        header.headerSize == kVersion2HeaderSize ? loadU32(data, kSurfaceCountOffset) : 1;
    return header;
}

struct Layout {
    std::string_view name;
    unsigned bits{};
    std::uint64_t redMask{};
    std::uint64_t greenMask{};
    std::uint64_t blueMask{};
    std::uint64_t alphaMask{};
};

// Masks of each pixel type as the PowerVR SDK documents them. Intensity types repeat one mask
// across the colour channels.
std::optional<Layout> layoutFor(PixelType type) {
    switch (type) {
    case PixelType::Argb4444:
        return Layout{"ARGB4444", 16, 0x0F00, 0x00F0, 0x000F, 0xF000};
    case PixelType::Argb1555:
        return Layout{"ARGB1555", 16, 0x7C00, 0x03E0, 0x001F, 0x8000};
    case PixelType::Rgb565:
    case PixelType::OpenGlRgb565:
        return Layout{"RGB565", 16, 0xF800, 0x07E0, 0x001F, 0};
    case PixelType::Rgb555:
        return Layout{"RGB555", 16, 0x7C00, 0x03E0, 0x001F, 0};
    case PixelType::Rgb888:
        return Layout{"RGB888", 24, 0xFF0000, 0x00FF00, 0x0000FF, 0};
    case PixelType::Argb8888:
        return Layout{"ARGB8888", 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000};
    case PixelType::Argb8332:
        return Layout{"ARGB8332", 16, 0x00E0, 0x001C, 0x0003, 0xFF00};
    case PixelType::Intensity8:
    case PixelType::OpenGlIntensity8:
        return Layout{"I8", 8, 0xFF, 0xFF, 0xFF, 0};
    case PixelType::AlphaIntensity88:
    case PixelType::OpenGlAlphaIntensity88:
        return Layout{"AI88", 16, 0x00FF, 0x00FF, 0x00FF, 0xFF00};
    case PixelType::OpenGlRgba4444:
        return Layout{"RGBA4444", 16, 0xF000, 0x0F00, 0x00F0, 0x000F};
    case PixelType::OpenGlRgba5551:
        return Layout{"RGBA5551", 16, 0xF800, 0x07C0, 0x003E, 0x0001};
    case PixelType::OpenGlRgba8888:
        return Layout{"RGBA8888", 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000};
    case PixelType::OpenGlRgb555:
        return Layout{"RGB555", 16, 0x001F, 0x03E0, 0x7C00, 0};
    case PixelType::OpenGlRgb888:
        return Layout{"RGB888", 24, 0x0000FF, 0x00FF00, 0xFF0000, 0};
    case PixelType::OpenGlBgra8888:
        return Layout{"BGRA8888", 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000};
    case PixelType::OpenGlAlpha8:
        return Layout{"A8", 8, 0, 0, 0, 0xFF};
    case PixelType::Rgb332:
        return Layout{"RGB332", 8, 0xE0, 0x1C, 0x03, 0};
    case PixelType::AlphaLuminance44:
        return Layout{"AL44", 8, 0x0F, 0x0F, 0x0F, 0xF0};
    default:
        return std::nullopt;
    }
}

std::optional<std::string_view> compressedName(PixelType type) {
    switch (type) {
    case PixelType::Pvrtc2:
    case PixelType::OpenGlPvrtc2:
        return "PVRTC 2bpp";
    case PixelType::Pvrtc4:
    case PixelType::OpenGlPvrtc4:
        return "PVRTC 4bpp";
    case PixelType::Dxt1:
        return "DXT1";
    case PixelType::Dxt2:
        return "DXT2";
    case PixelType::Dxt3:
        return "DXT3";
    case PixelType::Dxt4:
        return "DXT4";
    case PixelType::Dxt5:
        return "DXT5";
    default:
        return std::nullopt;
    }
}

// Every compressed type has a fixed rate. DXT2 to DXT5 store 16 bytes per 4x4 block.
std::uint32_t compressedBitsPerPixel(PixelType type) {
    switch (type) {
    case PixelType::Pvrtc2:
    case PixelType::OpenGlPvrtc2:
        return 2;
    case PixelType::Pvrtc4:
    case PixelType::OpenGlPvrtc4:
    case PixelType::Dxt1:
        return 4;
    default:
        return 8;
    }
}

std::expected<void, Error> decodeCompressed(Bytes body, PixelType type, Image &image) {
    switch (type) {
    case PixelType::Pvrtc2:
    case PixelType::OpenGlPvrtc2:
        return decodePvrtc(body, true, image);
    case PixelType::Pvrtc4:
    case PixelType::OpenGlPvrtc4:
        return decodePvrtc(body, false, image);
    case PixelType::Dxt1:
        return decodeBlocks(body, BlockFormat::Bc1Alpha, image);
    case PixelType::Dxt2:
    case PixelType::Dxt3:
        return decodeBlocks(body, BlockFormat::Bc2, image);
    default:
        return decodeBlocks(body, BlockFormat::Bc3, image);
    }
}

std::expected<void, Error>
decodeMasked(Bytes body, const Header &header, const Layout &layout, Image &image) {
    const unsigned bytesPerPixel = layout.bits / kBitsPerByte;
    const bool twiddled = (header.flags & kFlagTwiddled) != 0;
    if (twiddled && (!isPowerOfTwo(header.width) || !isPowerOfTwo(header.height))) {
        return fail(ErrorCode::Unsupported, "Twiddled textures must have power-of-two dimensions.");
    }
    const auto pixels =
        slice(body,
              0,
              static_cast<std::size_t>(header.width) * header.height * bytesPerPixel,
              "Pixel data");
    if (!pixels) {
        return std::unexpected(pixels.error());
    }
    const bool intensity = layout.redMask != 0 && layout.redMask == layout.greenMask &&
                           layout.greenMask == layout.blueMask;
    const bool alphaOnly = layout.redMask == 0 && layout.greenMask == 0 && layout.blueMask == 0;
    for (std::uint32_t y = 0; y < header.height; ++y) {
        for (std::uint32_t x = 0; x < header.width; ++x) {
            const auto index = twiddled ? twiddledIndex(x, y, header.width, header.height) :
                                          static_cast<std::size_t>(y) * header.width + x;
            const auto pixel = loadUint(*pixels, index * bytesPerPixel, bytesPerPixel);
            const auto alpha =
                layout.alphaMask != 0 ? maskedChannel(pixel, layout.alphaMask) : std::uint8_t{255};
            if (alphaOnly) {
                // Show an alpha-only texture as white with that alpha.
                setPixel(image, x, y, {255, 255, 255, alpha});
                continue;
            }
            const auto red = maskedChannel(pixel, layout.redMask);
            setPixel(image,
                     x,
                     y,
                     {red,
                      intensity ? red : maskedChannel(pixel, layout.greenMask),
                      intensity ? red : maskedChannel(pixel, layout.blueMask),
                      alpha});
        }
    }
    return {};
}

} // namespace

bool looksLikeLegacy(Bytes data) {
    if (!hasRange(data, 0, kVersion1HeaderSize)) {
        return false;
    }
    const auto headerSize = loadU32(data, 0);
    if (headerSize == kVersion2HeaderSize) {
        return hasRange(data, 0, kVersion2HeaderSize) && loadU32(data, kTagOffset) == kTag;
    }
    if (headerSize != kVersion1HeaderSize) {
        return false;
    }
    // Version 1 has no tag. Require a plausible header instead.
    const auto header = readHeader(data);
    return header.width != 0 && header.height != 0 && header.width <= kMaxPlausibleDimension &&
           header.height <= kMaxPlausibleDimension && header.bitCount != 0 &&
           header.bitCount <= kMaxPlausibleBitCount && header.dataSize != 0 &&
           header.dataSize <= data.size() - kVersion1HeaderSize;
}

std::expected<DecodeResult, Error> decodeLegacy(Bytes data, const Options &options) {
    if (!looksLikeLegacy(data)) {
        return fail(ErrorCode::Unrecognized, "Not a PowerVR legacy texture.");
    }
    const auto header = readHeader(data);
    const auto type = static_cast<PixelType>(header.flags & kPixelTypeMask);
    const auto body = data.subspan(header.headerSize);

    std::expected<Image, Error> image = fail(ErrorCode::Unsupported, "");
    if (const auto name = compressedName(type)) {
        image = makeImage(header.width, header.height, std::string(*name), options);
        if (!image) {
            return std::unexpected(image.error());
        }
        image->bitsPerPixel = compressedBitsPerPixel(type);
        if (const auto decoded = decodeCompressed(body, type, *image); !decoded) {
            return std::unexpected(decoded.error());
        }
        if (type == PixelType::Dxt2 || type == PixelType::Dxt4) {
            unpremultiply(*image);
        }
    } else {
        // Known types use their documented layout because some tools write wrong masks
        // (PVRTexTool labels RGB888 data as BGR). Other types use the header's masks.
        auto layout = layoutFor(type);
        if (!layout &&
            (header.redMask | header.greenMask | header.blueMask | header.alphaMask) != 0 &&
            header.bitCount != 0 && header.bitCount % kBitsPerByte == 0 &&
            header.bitCount <= kMaxPlausibleBitCount) {
            layout = Layout{"uncompressed",
                            header.bitCount,
                            header.redMask,
                            header.greenMask,
                            header.blueMask,
                            (header.flags & kFlagAlpha) != 0 ? header.alphaMask : 0};
        }
        if (!layout) {
            return fail(ErrorCode::Unsupported,
                        "Unsupported PowerVR pixel type " + hex(static_cast<unsigned>(type)) + ".");
        }
        image = makeImage(header.width, header.height, std::string(layout->name), options);
        if (!image) {
            return std::unexpected(image.error());
        }
        image->bitsPerPixel = layout->bits;
        if (const auto decoded = decodeMasked(body, header, *layout, *image); !decoded) {
            return std::unexpected(decoded.error());
        }
    }
    // Rows are stored top-down unless the vertical-flip flag is set. Files written for OpenGL are
    // sometimes stored bottom-up without the flag, and the header does not distinguish them.
    if ((header.flags & kFlagVerticalFlip) != 0) {
        flipVertical(*image);
    }
    if ((header.flags & kFlagMipmaps) != 0 && header.mipmapCount != 0) {
        image->format += ", " + std::to_string(header.mipmapCount) + " mipmaps";
    }
    if (header.surfaceCount > 1) {
        image->format += ", first of " + std::to_string(header.surfaceCount) + " surfaces";
    }
    DecodeResult result;
    result.container = Container::Legacy;
    result.images.push_back(std::move(*image));
    return result;
}

bool looksLikeApplePvrtc(Bytes data) {
    // texturetool writes square power-of-two textures without a header. The size alone gives the
    // dimensions, n * n / 2 bytes for 4 bpp (an odd power of two) or n * n / 4 for 2 bpp.
    return data.size() >= kApplePvrtcMinimumSize && isPowerOfTwo(data.size());
}

std::expected<DecodeResult, Error> decodeApplePvrtc(Bytes data, const Options &options) {
    if (!looksLikeApplePvrtc(data)) {
        return fail(ErrorCode::Unrecognized, "Not a headerless PVRTC texture.");
    }
    const auto log2Size = static_cast<unsigned>(std::countr_zero(data.size()));
    const bool twoBitsPerPixel = log2Size % 2 == 0;
    const auto pixels =
        data.size() * (twoBitsPerPixel ? kTwoBitPixelsPerByte : kFourBitPixelsPerByte);
    const auto side =
        static_cast<std::uint32_t>(std::lround(std::sqrt(static_cast<double>(pixels))));
    auto image = makeImage(side, side, twoBitsPerPixel ? "PVRTC 2bpp" : "PVRTC 4bpp", options);
    if (!image) {
        return std::unexpected(image.error());
    }
    image->bitsPerPixel = twoBitsPerPixel ? 2 : 4;
    if (const auto decoded = decodePvrtc(data, twoBitsPerPixel, *image); !decoded) {
        return std::unexpected(decoded.error());
    }
    DecodeResult result;
    result.container = Container::ApplePvrtc;
    result.images.push_back(std::move(*image));
    return result;
}

} // namespace Pvr::Detail
