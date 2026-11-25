// A Dreamcast texture is an optional GBIX chunk ("GBIX", u32 length, global index) followed by a
// PVRT chunk:
//
//     "PVRT"  u32 chunk length (bytes after this field)
//     u8 pixel format   u8 data format   u16 zero   u16 width   u16 height
//     texture data
//
// The pixel format gives the 16-bit texel encoding, or for palettised data the encoding of the
// palette entries. The data format gives the layout: twiddled (Morton order), rectangular, vector
// quantised against a codebook of 2x2 texel blocks, or 4 and 8-bit palette indices. Mipmapped
// textures store the levels smallest first, with the full-size level at the end of the chunk.
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "internal.h"

namespace Pvr::Detail {

namespace {

constexpr std::size_t kChunkHeaderSize = 16;
constexpr std::size_t kChunkLengthOffset = 4;
constexpr std::size_t kChunkLengthBase = 8;
constexpr std::size_t kPixelFormatOffset = 8;
constexpr std::size_t kDataFormatOffset = 9;
constexpr std::size_t kWidthOffset = 12;
constexpr std::size_t kHeightOffset = 14;
constexpr std::size_t kGlobalIndexHeaderSize = 8;
constexpr std::size_t kGlobalIndexLongSize = 8;
constexpr std::size_t kGlobalIndexShortSize = 4;
constexpr std::size_t kGlobalIndexPaddingSearchLimit = 32;
constexpr std::size_t kGlobalIndexPaddingAlignment = 4;
constexpr std::size_t kTexelSize = 2;
constexpr std::size_t kCodebookEntrySize = 8;
constexpr std::size_t kCodebookTexelsPerEntry = 4;
constexpr std::size_t kMaxCodebookEntries = 256;
constexpr std::size_t kPaletteHeaderSize = 16;
constexpr std::size_t kPaletteFormatOffset = 8;
constexpr std::size_t kPaletteCountOffset = 14;
constexpr std::size_t kPackEntrySize = 8;
constexpr std::size_t kPackMinimumSize = 16;
constexpr std::uint8_t kOpaque = 255;
constexpr unsigned kIndex4Colors = 16;
constexpr unsigned kIndex8Colors = 256;
constexpr std::size_t kPaddingTexels = 1;
constexpr std::size_t kAltPaddingTexels = 3;
constexpr std::size_t kIndexPaddingBytes = 1;

enum class PixelFormat : unsigned {
    Argb1555 = 0x00,
    Rgb565 = 0x01,
    Argb4444 = 0x02,
    Yuv422 = 0x03,
    Bump = 0x04,
    Rgb555 = 0x05,
    Argb8888 = 0x06, // Palette entries only.
};

enum class DataFormat : unsigned {
    SquareTwiddled = 0x01,
    SquareTwiddledMipmaps = 0x02,
    VectorQuantized = 0x03,
    VectorQuantizedMipmaps = 0x04,
    Index4 = 0x05,
    Index4Mipmaps = 0x06,
    Index8 = 0x07,
    Index8Mipmaps = 0x08,
    Rectangle = 0x09,
    RectangleMipmaps = 0x0A,
    Stride = 0x0B,
    StrideMipmaps = 0x0C,
    RectangleTwiddled = 0x0D,
    SmallVectorQuantized = 0x10,
    SmallVectorQuantizedMipmaps = 0x11,
    SquareTwiddledMipmapsAlt = 0x12,
};

enum class LayoutKind { Texels, VectorQuantized, Indices4, Indices8 };

struct Layout {
    std::string_view name;
    bool twiddled{};
    bool mipmaps{};
    LayoutKind kind{};
    bool smallCodebook{};
};

std::optional<std::string_view> pixelFormatName(PixelFormat format) {
    switch (format) {
    case PixelFormat::Argb1555:
        return "ARGB1555";
    case PixelFormat::Rgb565:
        return "RGB565";
    case PixelFormat::Argb4444:
        return "ARGB4444";
    case PixelFormat::Yuv422:
        return "YUV422";
    case PixelFormat::Bump:
        return "bump map";
    case PixelFormat::Rgb555:
        return "RGB555";
    case PixelFormat::Argb8888:
        return "ARGB8888";
    }
    return std::nullopt;
}

std::optional<Layout> layoutFor(DataFormat format) {
    switch (format) {
    case DataFormat::SquareTwiddled:
        return Layout{"twiddled", true, false, LayoutKind::Texels, false};
    case DataFormat::SquareTwiddledMipmaps:
    case DataFormat::SquareTwiddledMipmapsAlt:
        return Layout{"twiddled", true, true, LayoutKind::Texels, false};
    case DataFormat::RectangleTwiddled:
        return Layout{"twiddled rectangle", true, false, LayoutKind::Texels, false};
    case DataFormat::VectorQuantized:
        return Layout{"VQ", true, false, LayoutKind::VectorQuantized, false};
    case DataFormat::VectorQuantizedMipmaps:
        return Layout{"VQ", true, true, LayoutKind::VectorQuantized, false};
    case DataFormat::SmallVectorQuantized:
        return Layout{"small VQ", true, false, LayoutKind::VectorQuantized, true};
    case DataFormat::SmallVectorQuantizedMipmaps:
        return Layout{"small VQ", true, true, LayoutKind::VectorQuantized, true};
    case DataFormat::Index4:
        return Layout{"4bpp palettized", true, false, LayoutKind::Indices4, false};
    case DataFormat::Index4Mipmaps:
        return Layout{"4bpp palettized", true, true, LayoutKind::Indices4, false};
    case DataFormat::Index8:
        return Layout{"8bpp palettized", true, false, LayoutKind::Indices8, false};
    case DataFormat::Index8Mipmaps:
        return Layout{"8bpp palettized", true, true, LayoutKind::Indices8, false};
    case DataFormat::Rectangle:
        return Layout{"rectangle", false, false, LayoutKind::Texels, false};
    case DataFormat::RectangleMipmaps:
        return Layout{"rectangle", false, true, LayoutKind::Texels, false};
    case DataFormat::Stride:
        return Layout{"stride", false, false, LayoutKind::Texels, false};
    case DataFormat::StrideMipmaps:
        return Layout{"stride", false, true, LayoutKind::Texels, false};
    }
    return std::nullopt;
}

bool isPalettized(const Layout &layout) {
    return layout.kind == LayoutKind::Indices4 || layout.kind == LayoutKind::Indices8;
}

Color bumpColor(std::uint32_t texel) {
    // The high byte is the elevation (0 is flat, 255 is vertical) and the low byte the
    // rotation. The colour is the resulting unit normal mapped to [0, 255].
    constexpr double kUnitScale = 127.5;
    constexpr double kElevationRange = 255.0;
    constexpr double kRotationRange = 256.0;
    const double elevation = ((texel >> 8) & 0xFF) / kElevationRange * std::numbers::pi / 2;
    const double rotation = (texel & 0xFF) / kRotationRange * 2 * std::numbers::pi;
    const std::array normal{std::cos(elevation) * std::cos(rotation),
                            std::cos(elevation) * std::sin(rotation),
                            std::sin(elevation)};
    Color color{0, 0, 0, kOpaque};
    for (std::size_t i = 0; i < normal.size(); ++i) {
        color[i] = static_cast<std::uint8_t>(std::lround((normal[i] + 1) * kUnitScale));
    }
    return color;
}

Color texelColor(std::uint32_t texel, PixelFormat format) {
    switch (format) {
    case PixelFormat::Argb1555:
        return {expandBits((texel >> 10) & 0x1F, 5),
                expandBits((texel >> 5) & 0x1F, 5),
                expandBits(texel & 0x1F, 5),
                static_cast<std::uint8_t>((texel & 0x8000) != 0 ? kOpaque : 0)};
    case PixelFormat::Rgb565:
        return {expandBits((texel >> 11) & 0x1F, 5),
                expandBits((texel >> 5) & 0x3F, 6),
                expandBits(texel & 0x1F, 5),
                kOpaque};
    case PixelFormat::Argb4444:
        return {expandBits((texel >> 8) & 0xF, 4),
                expandBits((texel >> 4) & 0xF, 4),
                expandBits(texel & 0xF, 4),
                expandBits((texel >> 12) & 0xF, 4)};
    case PixelFormat::Rgb555:
        return {expandBits((texel >> 10) & 0x1F, 5),
                expandBits((texel >> 5) & 0x1F, 5),
                expandBits(texel & 0x1F, 5),
                kOpaque};
    case PixelFormat::Argb8888:
        return {static_cast<std::uint8_t>(texel >> 16),
                static_cast<std::uint8_t>(texel >> 8),
                static_cast<std::uint8_t>(texel),
                static_cast<std::uint8_t>(texel >> 24)};
    case PixelFormat::Bump:
        return bumpColor(texel);
    case PixelFormat::Yuv422:
        break;
    }
    return {0, 0, 0, kOpaque};
}

std::size_t levelTexels(std::uint32_t width, std::uint32_t height, unsigned level) {
    return static_cast<std::size_t>(std::max(1U, width >> level)) * std::max(1U, height >> level);
}

unsigned levelCount(std::uint32_t width, std::uint32_t height) {
    unsigned count = 1;
    while ((width >> count) != 0 || (height >> count) != 0) {
        ++count;
    }
    return count;
}

std::size_t levelSize(LayoutKind kind, std::size_t texels) {
    switch (kind) {
    case LayoutKind::Texels:
        return texels * kTexelSize;
    case LayoutKind::VectorQuantized:
        return std::max<std::size_t>(1, texels / kCodebookTexelsPerEntry);
    case LayoutKind::Indices4:
        return std::max<std::size_t>(1, texels / 2);
    case LayoutKind::Indices8:
        return texels;
    }
    return texels * kTexelSize;
}

// Size of the smaller levels and their padding as Sega's tools write them. The 1x1 level is
// padded by one texel for 16-bit data (three in the alternative layout) and by one byte for
// indices.
std::size_t
mipmapPrefixSize(LayoutKind kind, DataFormat format, std::uint32_t width, std::uint32_t height) {
    std::size_t total = 0;
    for (unsigned level = levelCount(width, height) - 1; level >= 1; --level) {
        total += levelSize(kind, levelTexels(width, height, level));
    }
    if (kind == LayoutKind::Texels) {
        total +=
            (format == DataFormat::SquareTwiddledMipmapsAlt ? kAltPaddingTexels : kPaddingTexels) *
            kTexelSize;
    } else {
        total += kIndexPaddingBytes;
    }
    return total;
}

std::size_t smallCodebookEntries(std::uint32_t width, bool mipmaps) {
    constexpr std::uint32_t kTinyWidth = 16;
    constexpr std::uint32_t kSmallWidth = 32;
    constexpr std::uint32_t kMediumWidth = 64;
    if (width <= kTinyWidth) {
        return 16;
    }
    if (mipmaps) {
        return width == kSmallWidth ? 64 : kMaxCodebookEntries;
    }
    if (width == kSmallWidth) {
        return 32;
    }
    return width == kMediumWidth ? 128 : kMaxCodebookEntries;
}

struct ChunkLocation {
    std::size_t offset{};
    std::optional<std::uint64_t> globalIndex;
};

std::optional<ChunkLocation> locateChunk(Bytes data) {
    if (startsWith(data, 0, "PVRT")) {
        return ChunkLocation{0, std::nullopt};
    }
    if (!startsWith(data, 0, "GBIX") || !hasRange(data, 0, kGlobalIndexHeaderSize)) {
        return std::nullopt;
    }
    const auto length = loadU32(data, 4);
    std::optional<std::uint64_t> globalIndex;
    if (length >= kGlobalIndexLongSize &&
        hasRange(data, kGlobalIndexHeaderSize, kGlobalIndexLongSize)) {
        globalIndex = loadU64(data, kGlobalIndexHeaderSize);
    } else if (length >= kGlobalIndexShortSize &&
               hasRange(data, kGlobalIndexHeaderSize, kGlobalIndexShortSize)) {
        globalIndex = loadU32(data, kGlobalIndexHeaderSize);
    }
    const auto expected = kGlobalIndexHeaderSize + static_cast<std::size_t>(length);
    if (startsWith(data, expected, "PVRT")) {
        return ChunkLocation{expected, globalIndex};
    }
    // Some tools pad the GBIX chunk.
    for (std::size_t offset = kGlobalIndexHeaderSize; offset <= kGlobalIndexPaddingSearchLimit;
         offset += kGlobalIndexPaddingAlignment) {
        if (startsWith(data, offset, "PVRT")) {
            return ChunkLocation{offset, globalIndex};
        }
    }
    return std::nullopt;
}

// Raw texel values (16-bit colours or palette indices) in row-major order.
std::expected<std::vector<std::uint32_t>, Error> gatherTexels(Bytes level,
                                                              Bytes codebook,
                                                              std::size_t codebookEntries,
                                                              const Layout &layout,
                                                              std::uint32_t width,
                                                              std::uint32_t height) {
    std::vector<std::uint32_t> raw(static_cast<std::size_t>(width) * height);
    const auto indexOf = [&](std::uint32_t x, std::uint32_t y) {
        return layout.twiddled ? twiddledIndex(x, y, width, height) :
                                 static_cast<std::size_t>(y) * width + x;
    };
    switch (layout.kind) {
    case LayoutKind::Texels:
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                raw[static_cast<std::size_t>(y) * width + x] =
                    loadU16(level, indexOf(x, y) * kTexelSize);
            }
        }
        break;
    case LayoutKind::VectorQuantized: {
        const auto blocksWide = width / 2;
        const auto blocksHigh = height / 2;
        for (std::uint32_t blockY = 0; blockY < blocksHigh; ++blockY) {
            for (std::uint32_t blockX = 0; blockX < blocksWide; ++blockX) {
                const std::size_t entry =
                    level[twiddledIndex(blockX, blockY, blocksWide, blocksHigh)];
                if (entry >= codebookEntries) {
                    return fail(ErrorCode::Invalid,
                                "VQ index " + std::to_string(entry) + " is outside the " +
                                    std::to_string(codebookEntries) + "-entry codebook.");
                }
                // An entry stores a twiddled 2x2 block as (0, 0), (0, 1), (1, 0), then (1, 1).
                for (std::uint32_t i = 0; i < kCodebookTexelsPerEntry; ++i) {
                    const auto x = blockX * 2 + i / 2;
                    const auto y = blockY * 2 + i % 2;
                    raw[static_cast<std::size_t>(y) * width + x] =
                        loadU16(codebook, (entry * kCodebookTexelsPerEntry + i) * kTexelSize);
                }
            }
        }
        break;
    }
    case LayoutKind::Indices4:
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                const auto index = indexOf(x, y);
                const auto byte = level[index / 2];
                raw[static_cast<std::size_t>(y) * width + x] =
                    index % 2 != 0 ? byte >> 4 : byte & 0xF;
            }
        }
        break;
    case LayoutKind::Indices8:
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                raw[static_cast<std::size_t>(y) * width + x] = level[indexOf(x, y)];
            }
        }
        break;
    }
    return raw;
}

void convertTexels(std::span<const std::uint32_t> raw,
                   const Layout &layout,
                   PixelFormat pixelFormat,
                   const Options &options,
                   Image &image) {
    const auto width = image.width;
    if (isPalettized(layout)) {
        const unsigned colors = layout.kind == LayoutKind::Indices4 ? kIndex4Colors : kIndex8Colors;
        if (!options.palette) {
            image.format += " (no palette, shown as grayscale)";
        }
        for (std::size_t i = 0; i < raw.size(); ++i) {
            const auto index = raw[i];
            Color color;
            if (options.palette) {
                color = index < options.palette->size() ? (*options.palette)[index] : Color{};
            } else {
                const auto gray = static_cast<std::uint8_t>(index * kOpaque / (colors - 1));
                color = {gray, gray, gray, kOpaque};
            }
            setPixel(image,
                     static_cast<std::uint32_t>(i % width),
                     static_cast<std::uint32_t>(i / width),
                     color);
        }
        return;
    }
    if (pixelFormat == PixelFormat::Yuv422) {
        // Horizontally adjacent texels share chroma (Y0 with U, then Y1 with V). Y is the high
        // byte.
        for (std::uint32_t y = 0; y < image.height; ++y) {
            for (std::uint32_t x = 0; x < width; x += 2) {
                const auto first = raw[static_cast<std::size_t>(y) * width + x];
                const auto second =
                    x + 1 < width ? raw[static_cast<std::size_t>(y) * width + x + 1] : first;
                const auto blueDifference = static_cast<int>(first & 0xFF);
                const auto redDifference = static_cast<int>(second & 0xFF);
                setPixel(image,
                         x,
                         y,
                         yuvToRgb(static_cast<int>(first >> 8), blueDifference, redDifference));
                if (x + 1 < width) {
                    setPixel(
                        image,
                        x + 1,
                        y,
                        yuvToRgb(static_cast<int>(second >> 8), blueDifference, redDifference));
                }
            }
        }
        return;
    }
    for (std::size_t i = 0; i < raw.size(); ++i) {
        setPixel(image,
                 static_cast<std::uint32_t>(i % width),
                 static_cast<std::uint32_t>(i / width),
                 texelColor(raw[i], pixelFormat));
    }
}

std::expected<Image, Error>
decodeChunk(Bytes data, const ChunkLocation &location, const Options &options) {
    const auto header = slice(data, location.offset, kChunkHeaderSize, "PVRT header");
    if (!header) {
        return std::unexpected(header.error());
    }
    const auto chunkLength = loadU32(*header, kChunkLengthOffset);
    const auto pixelFormat = static_cast<PixelFormat>((*header)[kPixelFormatOffset]);
    const auto dataFormat = static_cast<DataFormat>((*header)[kDataFormatOffset]);
    const std::uint32_t width = loadU16(*header, kWidthOffset);
    const std::uint32_t height = loadU16(*header, kHeightOffset);

    const auto pixelName = pixelFormatName(pixelFormat);
    if (!pixelName) {
        return fail(ErrorCode::Unsupported,
                    "Unsupported Dreamcast pixel format " +
                        hex(static_cast<unsigned>(pixelFormat)) + ".");
    }
    const auto layout = layoutFor(dataFormat);
    if (!layout) {
        return fail(ErrorCode::Unsupported,
                    "Unsupported Dreamcast data format " + hex(static_cast<unsigned>(dataFormat)) +
                        ".");
    }
    if (pixelFormat == PixelFormat::Argb8888 && !isPalettized(*layout)) {
        return fail(ErrorCode::Invalid,
                    "ARGB8888 is only valid for palettized Dreamcast textures.");
    }
    if (layout->twiddled && (!isPowerOfTwo(width) || !isPowerOfTwo(height))) {
        return fail(ErrorCode::Invalid, "Twiddled textures must have power-of-two dimensions.");
    }
    if (layout->kind == LayoutKind::VectorQuantized && (width < 2 || height < 2)) {
        return fail(ErrorCode::Invalid, "VQ textures must be at least 2x2.");
    }

    std::string format = std::string(*pixelName) + ", " + std::string(layout->name);
    if (layout->mipmaps) {
        format += ", mipmaps";
    }
    if (location.globalIndex) {
        format += ", global index " + std::to_string(*location.globalIndex);
    }
    auto image = makeImage(width, height, std::move(format), options);
    if (!image) {
        return image;
    }

    // Texture data runs from the end of the header to the end of the chunk. Clamp to the bytes
    // present because packs may record a longer chunk.
    const auto start = location.offset + kChunkHeaderSize;
    const auto declaredEnd =
        static_cast<std::uint64_t>(location.offset) + kChunkLengthBase + chunkLength;
    const bool truncated = declaredEnd > data.size() || declaredEnd < start;
    const auto end = truncated ? data.size() : static_cast<std::size_t>(declaredEnd);
    const auto body = data.subspan(start, end - start);

    std::size_t codebookEntries = 0;
    const auto topSize = levelSize(layout->kind, static_cast<std::size_t>(width) * height);
    if (layout->kind == LayoutKind::VectorQuantized) {
        codebookEntries = kMaxCodebookEntries;
        if (layout->smallCodebook) {
            codebookEntries = smallCodebookEntries(width, layout->mipmaps);
            // Without mipmaps the entry count follows from the data size; prefer it when the
            // data divides evenly.
            if (!layout->mipmaps && body.size() > topSize &&
                (body.size() - topSize) % kCodebookEntrySize == 0 &&
                (body.size() - topSize) / kCodebookEntrySize <= kMaxCodebookEntries) {
                codebookEntries = (body.size() - topSize) / kCodebookEntrySize;
            }
        }
    }
    const auto codebookSize = codebookEntries * kCodebookEntrySize;
    auto topOffset = codebookSize;
    if (layout->mipmaps) {
        topOffset = !truncated && body.size() >= codebookSize + topSize ?
                        body.size() - topSize :
                        codebookSize + mipmapPrefixSize(layout->kind, dataFormat, width, height);
    }
    const auto codebook = slice(body, 0, codebookSize, "VQ codebook");
    if (!codebook) {
        return std::unexpected(codebook.error());
    }
    const auto level = slice(body, topOffset, topSize, "Full-size texture level");
    if (!level) {
        return std::unexpected(level.error());
    }
    const auto raw = gatherTexels(*level, *codebook, codebookEntries, *layout, width, height);
    if (!raw) {
        return std::unexpected(raw.error());
    }
    convertTexels(*raw, *layout, pixelFormat, options, *image);
    return image;
}

} // namespace

bool looksLikeDreamcast(Bytes data) {
    return locateChunk(data).has_value();
}

std::expected<DecodeResult, Error> decodeDreamcast(Bytes data, const Options &options) {
    const auto location = locateChunk(data);
    if (!location) {
        return fail(ErrorCode::Unrecognized, "Not a Dreamcast PVRT texture.");
    }
    auto image = decodeChunk(data, *location, options);
    if (!image) {
        return std::unexpected(image.error());
    }
    DecodeResult result;
    result.container = Container::Dreamcast;
    result.images.push_back(std::move(*image));
    return result;
}

bool looksLikeDreamcastPack(Bytes data) {
    if (!hasRange(data, 0, kPackMinimumSize)) {
        return false;
    }
    const auto tableLength = loadU32(data, 0);
    if (tableLength < kPackEntrySize || tableLength % kPackEntrySize != 0 ||
        tableLength > data.size()) {
        return false;
    }
    return locateChunk(data.subspan(tableLength)).has_value();
}

std::expected<DecodeResult, Error> decodeDreamcastPack(Bytes data, const Options &options) {
    if (!looksLikeDreamcastPack(data)) {
        return fail(ErrorCode::Unrecognized, "Not a Dreamcast texture pack.");
    }
    const auto tableLength = loadU32(data, 0);
    DecodeResult result;
    result.container = Container::DreamcastPack;
    const auto record = [&result](std::size_t position, Error error) {
        result.failures.push_back(PackFailure{position, std::move(error)});
    };
    for (std::size_t position = 0; position < tableLength / kPackEntrySize; ++position) {
        const auto offset = loadU32(data, position * kPackEntrySize);
        const auto size = loadU32(data, position * kPackEntrySize + 4);
        if (offset == 0 && size == 0) {
            break;
        }
        if (offset < tableLength || offset >= data.size()) {
            record(position,
                   Error{ErrorCode::Invalid,
                         "Entry offset " + std::to_string(offset) + " is outside the pack."});
            continue;
        }
        // The last entry often records more bytes than the file has; clamp to the end.
        const auto entry = data.subspan(offset, std::min<std::size_t>(size, data.size() - offset));
        const auto location = locateChunk(entry);
        if (!location) {
            record(position,
                   Error{ErrorCode::Invalid, "The entry does not start with a PVRT chunk."});
            continue;
        }
        auto image = decodeChunk(entry, *location, options);
        if (!image) {
            record(position, image.error());
            continue;
        }
        image->format = "texture " + std::to_string(position) + ": " + image->format;
        result.images.push_back(std::move(*image));
    }
    if (result.images.empty()) {
        if (result.failures.empty()) {
            return fail(ErrorCode::Invalid, "The pack has no textures.");
        }
        return fail(result.failures.front().error.code,
                    "No texture in the pack could be decoded. " +
                        result.failures.front().error.message);
    }
    return result;
}

} // namespace Pvr::Detail

namespace Pvr {

std::expected<std::vector<Color>, Error> decodePalette(std::span<const std::uint8_t> data) {
    using namespace Detail;
    if (!startsWith(data, 0, "PVPL") || !hasRange(data, 0, kPaletteHeaderSize)) {
        return fail(ErrorCode::Unrecognized, "Not a Dreamcast PVPL palette.");
    }
    const auto declaredEnd =
        std::min<std::uint64_t>(kChunkLengthBase + loadU32(data, kChunkLengthOffset), data.size());
    const auto format = static_cast<PixelFormat>(data[kPaletteFormatOffset]);
    switch (format) {
    case PixelFormat::Argb1555:
    case PixelFormat::Rgb565:
    case PixelFormat::Argb4444:
    case PixelFormat::Rgb555:
    case PixelFormat::Argb8888:
        break;
    default:
        return fail(ErrorCode::Unsupported,
                    "Unsupported palette pixel format " + hex(static_cast<unsigned>(format)) + ".");
    }
    const std::size_t entrySize = format == PixelFormat::Argb8888 ? 4 : 2;
    const auto available =
        declaredEnd > kPaletteHeaderSize ?
            static_cast<std::size_t>(declaredEnd - kPaletteHeaderSize) / entrySize :
            0;
    std::size_t count = loadU16(data, kPaletteCountOffset);
    if (count == 0 || count > available) {
        count = available;
    }
    if (count == 0) {
        return fail(ErrorCode::Invalid, "The palette has no entries.");
    }
    std::vector<Color> colors;
    colors.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto value = static_cast<std::uint32_t>(
            loadUint(data, kPaletteHeaderSize + i * entrySize, static_cast<unsigned>(entrySize)));
        colors.push_back(texelColor(value, format));
    }
    return colors;
}

} // namespace Pvr
