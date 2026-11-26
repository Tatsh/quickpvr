#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>

#include "internal.h"

namespace Pvr::Detail {

namespace {

constexpr std::size_t kHeaderSize = 52;
constexpr std::size_t kFlagsOffset = 4;
constexpr std::size_t kPixelFormatOffset = 8;
constexpr std::size_t kChannelTypeOffset = 20;
constexpr std::size_t kHeightOffset = 24;
constexpr std::size_t kWidthOffset = 28;
constexpr std::size_t kDepthOffset = 32;
constexpr std::size_t kSurfaceCountOffset = 36;
constexpr std::size_t kFaceCountOffset = 40;
constexpr std::size_t kMipmapCountOffset = 44;
constexpr std::size_t kMetadataSizeOffset = 48;
constexpr std::size_t kMetadataEntryHeaderSize = 12;
constexpr std::uint32_t kFlagPremultiplied = 0x02;
constexpr std::uint32_t kMetadataFourCc = 0x03525650; // "PVR\x03"
constexpr std::uint32_t kMetadataOrientationKey = 3;
constexpr unsigned kMaxChannels = 4;
constexpr unsigned kBitsPerByte = 8;
constexpr unsigned kMaxPixelBits = 64;
constexpr unsigned kHalfFloatBits = 16;
constexpr unsigned kFloatBits = 32;
constexpr std::uint8_t kOpaque = 255;

enum class ChannelType : std::uint32_t {
    UnsignedByteNormalized = 0,
    SignedByteNormalized = 1,
    UnsignedByte = 2,
    SignedByte = 3,
    UnsignedShortNormalized = 4,
    SignedShortNormalized = 5,
    UnsignedShort = 6,
    SignedShort = 7,
    UnsignedIntegerNormalized = 8,
    SignedIntegerNormalized = 9,
    UnsignedInteger = 10,
    SignedInteger = 11,
    SignedFloat = 12,
    UnsignedFloat = 13,
};

// Compressed formats are identified by the low 32 bits when the high 32 bits are zero.
enum class CompressedFormat : std::uint32_t {
    Pvrtc2Rgb = 0,
    Pvrtc2Rgba = 1,
    Pvrtc4Rgb = 2,
    Pvrtc4Rgba = 3,
    Bc1 = 7,
    Dxt2 = 8,
    Bc2 = 9,
    Dxt4 = 10,
    Bc3 = 11,
    Bc4 = 12,
    Bc5 = 13,
    Uyvy = 16,
    Yuy2 = 17,
    Rgbg8888 = 20,
    Grgb8888 = 21,
};

constexpr std::array kCompressedNames = {"PVRTC 2bpp RGB",
                                         "PVRTC 2bpp RGBA",
                                         "PVRTC 4bpp RGB",
                                         "PVRTC 4bpp RGBA",
                                         "PVRTC-II 2bpp",
                                         "PVRTC-II 4bpp",
                                         "ETC1",
                                         "BC1",
                                         "DXT2",
                                         "BC2",
                                         "DXT4",
                                         "BC3",
                                         "BC4",
                                         "BC5",
                                         "BC6",
                                         "BC7",
                                         "UYVY",
                                         "YUY2",
                                         "1bpp",
                                         "RGB9E5",
                                         "RGBG8888",
                                         "GRGB8888",
                                         "ETC2 RGB",
                                         "ETC2 RGBA",
                                         "ETC2 RGB A1",
                                         "EAC R11",
                                         "EAC R11 signed",
                                         "EAC RG11",
                                         "EAC RG11 signed"};

std::uint8_t fromHalfFloat(std::uint16_t half) {
    constexpr unsigned kExponentShift = 10;
    constexpr unsigned kExponentMask = 0x1F;
    constexpr unsigned kMantissaMask = 0x3FF;
    constexpr unsigned kSignBit = 0x8000;
    constexpr unsigned kInfinityExponent = 31;
    constexpr int kSubnormalScale = -24;
    constexpr int kExponentBias = 25;
    constexpr unsigned kImplicitBit = 1024;
    if ((half & kSignBit) != 0) {
        return 0;
    }
    const auto exponent = (half >> kExponentShift) & kExponentMask;
    const auto mantissa = half & kMantissaMask;
    double value;
    if (exponent == 0) {
        value = std::ldexp(mantissa, kSubnormalScale);
    } else if (exponent == kInfinityExponent) {
        value = mantissa != 0 ? 0 : 1;
    } else {
        value = std::ldexp(mantissa + kImplicitBit, static_cast<int>(exponent) - kExponentBias);
    }
    return static_cast<std::uint8_t>(std::lround(std::fmin(1.0, value) * kOpaque));
}

std::uint8_t fromFloat(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    if (!(value > 0)) {
        return 0;
    }
    return static_cast<std::uint8_t>(std::lround(std::fmin(1.0F, value) * kOpaque));
}

bool isSigned(ChannelType type) {
    switch (type) {
    case ChannelType::SignedByteNormalized:
    case ChannelType::SignedByte:
    case ChannelType::SignedShortNormalized:
    case ChannelType::SignedShort:
    case ChannelType::SignedIntegerNormalized:
    case ChannelType::SignedInteger:
        return true;
    default:
        return false;
    }
}

std::expected<std::uint8_t, Error>
convertChannel(std::uint64_t value, unsigned bits, ChannelType type) {
    if (type == ChannelType::SignedFloat || type == ChannelType::UnsignedFloat) {
        if (bits == kHalfFloatBits) {
            return fromHalfFloat(static_cast<std::uint16_t>(value));
        }
        if (bits == kFloatBits) {
            return fromFloat(static_cast<std::uint32_t>(value));
        }
        return fail(ErrorCode::Unsupported,
                    "Unsupported floating-point channel width " + std::to_string(bits) + ".");
    }
    if (isSigned(type)) {
        // Map [-maximum, maximum] to [0, 255] so signed data such as normal maps stays visible.
        const auto sign = std::uint64_t{1} << (bits - 1);
        return expandBits((value ^ sign) & ((sign << 1) - 1), bits);
    }
    return expandBits(value, bits);
}

struct UncompressedFormat {
    std::array<char, kMaxChannels> names{};
    std::array<unsigned, kMaxChannels> bits{};
    unsigned count{};
    unsigned totalBits{};
};

std::string describe(const UncompressedFormat &format) {
    std::string out;
    for (unsigned i = 0; i < format.count; ++i) {
        out += static_cast<char>(std::toupper(static_cast<unsigned char>(format.names[i])));
    }
    for (unsigned i = 0; i < format.count; ++i) {
        out += std::to_string(format.bits[i]);
    }
    return out;
}

void assignChannel(char name, std::uint8_t value, Color &color) {
    switch (name) {
    case 'r':
        color[0] = value;
        break;
    case 'g':
        color[1] = value;
        break;
    case 'b':
        color[2] = value;
        break;
    case 'a':
        color[3] = value;
        break;
    case 'l':
    case 'd':
        color[0] = value;
        color[1] = value;
        color[2] = value;
        break;
    case 'i':
        color.fill(value);
        break;
    default:
        // Padding ('x'), stencil ('s'), and unknown channels are not shown.
        break;
    }
}

std::expected<void, Error>
decodeUncompressed(Bytes body, const UncompressedFormat &format, ChannelType type, Image &image) {
    const unsigned bytesPerPixel = format.totalBits / kBitsPerByte;
    bool byteAligned = true;
    for (unsigned i = 0; i < format.count; ++i) {
        byteAligned = byteAligned && format.bits[i] % kBitsPerByte == 0;
    }
    const auto pixels = slice(body,
                              0,
                              static_cast<std::size_t>(image.width) * image.height * bytesPerPixel,
                              "Pixel data");
    if (!pixels) {
        return std::unexpected(pixels.error());
    }
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * image.width + x) * bytesPerPixel;
            const auto word = byteAligned ? 0 : loadUint(*pixels, offset, bytesPerPixel);
            Color color{0, 0, 0, kOpaque};
            unsigned position = 0;
            for (unsigned i = 0; i < format.count; ++i) {
                const auto bits = format.bits[i];
                // Byte-aligned channels are stored in the order named. Packed channels put the
                // first channel named in the most significant bits.
                const auto value =
                    byteAligned ?
                        loadUint(*pixels, offset + position / kBitsPerByte, bits / kBitsPerByte) :
                        (word >> (format.totalBits - position - bits)) &
                            ((std::uint64_t{1} << bits) - 1);
                position += bits;
                const auto converted = convertChannel(value, bits, type);
                if (!converted) {
                    return std::unexpected(converted.error());
                }
                assignChannel(format.names[i], *converted, color);
            }
            setPixel(image, x, y, color);
        }
    }
    return {};
}

// In RGBG8888 and GRGB8888, two horizontally adjacent pixels share red and blue.
std::expected<void, Error> decodeSharedRedBlue(Bytes body, bool greenFirst, Image &image) {
    constexpr std::size_t kBytesPerPair = 4;
    const auto pairs = (static_cast<std::size_t>(image.width) + 1) / 2;
    const auto pixels = slice(body, 0, pairs * kBytesPerPair * image.height, "Pixel data");
    if (!pixels) {
        return std::unexpected(pixels.error());
    }
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::size_t pair = 0; pair < pairs; ++pair) {
            const auto *p = pixels->data() + (y * pairs + pair) * kBytesPerPair;
            const std::uint8_t red = greenFirst ? p[1] : p[0];
            const std::uint8_t blue = greenFirst ? p[3] : p[2];
            const std::uint8_t green0 = greenFirst ? p[0] : p[1];
            const std::uint8_t green1 = greenFirst ? p[2] : p[3];
            const auto x = static_cast<std::uint32_t>(pair * 2);
            setPixel(image, x, y, {red, green0, blue, kOpaque});
            if (x + 1 < image.width) {
                setPixel(image, x + 1, y, {red, green1, blue, kOpaque});
            }
        }
    }
    return {};
}

std::expected<void, Error> decodeCompressed(Bytes body, CompressedFormat format, Image &image) {
    switch (format) {
    case CompressedFormat::Pvrtc2Rgb:
    case CompressedFormat::Pvrtc2Rgba:
        return decodePvrtc(body, true, image);
    case CompressedFormat::Pvrtc4Rgb:
    case CompressedFormat::Pvrtc4Rgba:
        return decodePvrtc(body, false, image);
    case CompressedFormat::Bc1:
        return decodeBlocks(body, BlockFormat::Bc1Alpha, image);
    case CompressedFormat::Dxt2:
    case CompressedFormat::Bc2:
        return decodeBlocks(body, BlockFormat::Bc2, image);
    case CompressedFormat::Dxt4:
    case CompressedFormat::Bc3:
        return decodeBlocks(body, BlockFormat::Bc3, image);
    case CompressedFormat::Bc4:
        return decodeBlocks(body, BlockFormat::Bc4, image);
    case CompressedFormat::Bc5:
        return decodeBlocks(body, BlockFormat::Bc5, image);
    case CompressedFormat::Uyvy:
        return decodeYuv422(body, true, image);
    case CompressedFormat::Yuy2:
        return decodeYuv422(body, false, image);
    case CompressedFormat::Rgbg8888:
        return decodeSharedRedBlue(body, false, image);
    case CompressedFormat::Grgb8888:
        return decodeSharedRedBlue(body, true, image);
    }
    return fail(ErrorCode::Unsupported, "Unsupported PVR v3 compressed format.");
}

bool isDecodable(std::uint32_t id) {
    switch (static_cast<CompressedFormat>(id)) {
    case CompressedFormat::Pvrtc2Rgb:
    case CompressedFormat::Pvrtc2Rgba:
    case CompressedFormat::Pvrtc4Rgb:
    case CompressedFormat::Pvrtc4Rgba:
    case CompressedFormat::Bc1:
    case CompressedFormat::Dxt2:
    case CompressedFormat::Bc2:
    case CompressedFormat::Dxt4:
    case CompressedFormat::Bc3:
    case CompressedFormat::Bc4:
    case CompressedFormat::Bc5:
    case CompressedFormat::Uyvy:
    case CompressedFormat::Yuy2:
    case CompressedFormat::Rgbg8888:
    case CompressedFormat::Grgb8888:
        return true;
    }
    return false;
}

// The packed YUV and RGBG formats store two pixels in 32 bits. BC2, BC3, and BC5 store 16 bytes
// per 4x4 block, and the others 8 bytes.
std::uint32_t compressedBitsPerPixel(CompressedFormat format) {
    switch (format) {
    case CompressedFormat::Pvrtc2Rgb:
    case CompressedFormat::Pvrtc2Rgba:
        return 2;
    case CompressedFormat::Pvrtc4Rgb:
    case CompressedFormat::Pvrtc4Rgba:
    case CompressedFormat::Bc1:
    case CompressedFormat::Bc4:
        return 4;
    case CompressedFormat::Dxt2:
    case CompressedFormat::Bc2:
    case CompressedFormat::Dxt4:
    case CompressedFormat::Bc3:
    case CompressedFormat::Bc5:
        return 8;
    case CompressedFormat::Uyvy:
    case CompressedFormat::Yuy2:
    case CompressedFormat::Rgbg8888:
    case CompressedFormat::Grgb8888:
        return 16;
    }
    return 0;
}

struct Orientation {
    bool flipX{};
    bool flipY{};
};

// Orientation metadata has one byte per axis. A non-zero Y means rows run bottom-up and a
// non-zero X means columns run right to left.
Orientation readOrientation(Bytes metadata) {
    Orientation orientation;
    std::size_t offset = 0;
    while (hasRange(metadata, offset, kMetadataEntryHeaderSize)) {
        const auto fourCc = loadU32(metadata, offset);
        const auto key = loadU32(metadata, offset + 4);
        const auto size = loadU32(metadata, offset + 8);
        if (!hasRange(metadata, offset + kMetadataEntryHeaderSize, size)) {
            break;
        }
        if (fourCc == kMetadataFourCc && key == kMetadataOrientationKey && size >= 2) {
            orientation.flipX = metadata[offset + kMetadataEntryHeaderSize] != 0;
            orientation.flipY = metadata[offset + kMetadataEntryHeaderSize + 1] != 0;
        }
        offset += kMetadataEntryHeaderSize + size;
    }
    return orientation;
}

std::optional<UncompressedFormat> parseUncompressed(std::uint64_t pixelFormat) {
    constexpr unsigned kBitCountShift = 32;
    UncompressedFormat format;
    for (unsigned i = 0; i < kMaxChannels; ++i) {
        const auto name = static_cast<char>((pixelFormat >> (kBitsPerByte * i)) & 0xFF);
        const auto bits =
            static_cast<unsigned>((pixelFormat >> (kBitCountShift + kBitsPerByte * i)) & 0xFF);
        if (name == 0 || bits == 0) {
            break;
        }
        format.names[format.count] =
            static_cast<char>(std::tolower(static_cast<unsigned char>(name)));
        format.bits[format.count] = bits;
        format.totalBits += bits;
        ++format.count;
    }
    if (format.count == 0 || format.totalBits % kBitsPerByte != 0 ||
        format.totalBits > kMaxPixelBits) {
        return std::nullopt;
    }
    return format;
}

} // namespace

std::expected<DecodeResult, Error> decodeVersion3(Bytes data, const Options &options) {
    const auto header = slice(data, 0, kHeaderSize, "PVR v3 header");
    if (!header) {
        return std::unexpected(header.error());
    }
    if (!startsWith(data, 0, "PVR\x03")) {
        return fail(ErrorCode::Unrecognized, "Not a PVR v3 texture.");
    }
    const auto flags = loadU32(*header, kFlagsOffset);
    const auto pixelFormat = loadU64(*header, kPixelFormatOffset);
    const auto channelType = static_cast<ChannelType>(loadU32(*header, kChannelTypeOffset));
    const auto height = loadU32(*header, kHeightOffset);
    const auto width = loadU32(*header, kWidthOffset);
    const auto depth = loadU32(*header, kDepthOffset);
    const auto surfaceCount = loadU32(*header, kSurfaceCountOffset);
    const auto faceCount = loadU32(*header, kFaceCountOffset);
    const auto mipmapCount = loadU32(*header, kMipmapCountOffset);
    const auto metadataSize = loadU32(*header, kMetadataSizeOffset);
    const auto metadata = slice(data, kHeaderSize, metadataSize, "PVR v3 metadata");
    if (!metadata) {
        return std::unexpected(metadata.error());
    }
    const auto orientation = readOrientation(*metadata);
    const auto body = data.subspan(kHeaderSize + metadataSize);

    std::expected<Image, Error> image = fail(ErrorCode::Unsupported, "");
    constexpr unsigned kBitCountShift = 32;
    if ((pixelFormat >> kBitCountShift) == 0) {
        const auto id = static_cast<std::uint32_t>(pixelFormat);
        const std::string name =
            id < kCompressedNames.size() ? kCompressedNames[id] : "format " + std::to_string(id);
        if (!isDecodable(id)) {
            return fail(ErrorCode::Unsupported,
                        "Unsupported PVR v3 compressed format " + name + ".");
        }
        image = makeImage(width, height, name, options);
        if (!image) {
            return std::unexpected(image.error());
        }
        const auto format = static_cast<CompressedFormat>(id);
        image->bitsPerPixel = compressedBitsPerPixel(format);
        if (const auto decoded = decodeCompressed(body, format, *image); !decoded) {
            return std::unexpected(decoded.error());
        }
        if (format == CompressedFormat::Dxt2 || format == CompressedFormat::Dxt4) {
            unpremultiply(*image);
        }
    } else {
        const auto format = parseUncompressed(pixelFormat);
        if (!format) {
            return fail(ErrorCode::Unsupported, "Unsupported PVR v3 pixel format.");
        }
        image = makeImage(width, height, describe(*format), options);
        if (!image) {
            return std::unexpected(image.error());
        }
        image->bitsPerPixel = format->totalBits;
        if (const auto decoded = decodeUncompressed(body, *format, channelType, *image); !decoded) {
            return std::unexpected(decoded.error());
        }
    }
    if ((flags & kFlagPremultiplied) != 0) {
        unpremultiply(*image);
    }
    if (orientation.flipY) {
        flipVertical(*image);
    }
    if (orientation.flipX) {
        flipHorizontal(*image);
    }
    if (mipmapCount > 1) {
        image->format += ", " + std::to_string(mipmapCount) + " mipmap levels";
    }
    if (surfaceCount > 1 || faceCount > 1 || depth > 1) {
        image->format += ", first slice";
    }
    DecodeResult result;
    result.container = Container::Version3;
    result.images.push_back(std::move(*image));
    return result;
}

} // namespace Pvr::Detail
