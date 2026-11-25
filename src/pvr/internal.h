#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "pvr.h"

/** Helpers shared by the decoders. */
namespace Pvr::Detail {

/** Read-only view of encoded data. */
using Bytes = std::span<const std::uint8_t>;

/** Build an error to return through `std::expected`. */
std::unexpected<Error> fail(ErrorCode code, std::string message);

/** Format a value as two or more lower-case hexadecimal digits with a `0x` prefix. */
std::string hex(unsigned value);

/** Whether `[offset, offset + length)` lies inside @p data. */
inline bool hasRange(Bytes data, std::size_t offset, std::size_t length) {
    return offset <= data.size() && length <= data.size() - offset;
}

/**
 * The bytes `[offset, offset + length)` of @p data.
 *
 * @param what Description of the bytes, used in the error message.
 * @return The bytes, or an `ErrorCode::Truncated` error.
 */
std::expected<Bytes, Error>
slice(Bytes data, std::size_t offset, std::size_t length, std::string_view what);

/** Whether @p data has @p magic at @p offset. */
inline bool startsWith(Bytes data, std::size_t offset, std::string_view magic) {
    return hasRange(data, offset, magic.size()) &&
           std::memcmp(data.data() + offset, magic.data(), magic.size()) == 0;
}

/** Little-endian value of @p byteCount bytes (1 to 8). The range must lie inside @p data. */
inline std::uint64_t loadUint(Bytes data, std::size_t offset, unsigned byteCount) {
    assert(hasRange(data, offset, byteCount));
    std::uint64_t value = 0;
    for (unsigned i = 0; i < byteCount; ++i) {
        value |= static_cast<std::uint64_t>(data[offset + i]) << (8 * i);
    }
    return value;
}

/** Little-endian 16-bit value. The range must lie inside @p data. */
inline std::uint16_t loadU16(Bytes data, std::size_t offset) {
    return static_cast<std::uint16_t>(loadUint(data, offset, 2));
}

/** Little-endian 32-bit value. The range must lie inside @p data. */
inline std::uint32_t loadU32(Bytes data, std::size_t offset) {
    return static_cast<std::uint32_t>(loadUint(data, offset, 4));
}

/** Little-endian 64-bit value. The range must lie inside @p data. */
inline std::uint64_t loadU64(Bytes data, std::size_t offset) {
    return loadUint(data, offset, 8);
}

/**
 * Allocate a transparent black image.
 *
 * @return The image, or an `ErrorCode::Invalid` (zero dimension) or `ErrorCode::TooLarge` error.
 */
std::expected<Image, Error>
makeImage(std::uint32_t width, std::uint32_t height, std::string format, const Options &options);

/** Store one pixel. The coordinates must lie inside the image. */
inline void setPixel(Image &image, std::uint32_t x, std::uint32_t y, const Color &color) {
    assert(x < image.width && y < image.height);
    const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4;
    std::memcpy(image.rgba.data() + offset, color.data(), color.size());
}

/** Widen a field of @p bits bits to 8 bits by bit replication. All ones become 255. */
std::uint8_t expandBits(std::uint64_t value, unsigned bits);

/** The channel selected by @p mask in @p pixel, widened to 8 bits. A zero mask yields 255. */
std::uint8_t maskedChannel(std::uint64_t pixel, std::uint64_t mask);

/** Whether @p value is a power of two. Zero is not. */
inline bool isPowerOfTwo(std::uint64_t value) {
    return std::has_single_bit(value);
}

/**
 * Offset of texel `(x, y)` in a twiddled (Morton order) texture. A non-square texture is a row or
 * column of square twiddled blocks. Both dimensions must be powers of two. The offset is then
 * below `width * height`.
 */
std::size_t
twiddledIndex(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height);

/** Reverse the row order. */
void flipVertical(Image &image);
/** Reverse the column order. */
void flipHorizontal(Image &image);
/** Convert premultiplied alpha to straight alpha. */
void unpremultiply(Image &image);

/** Convert one full-range BT.601 sample to an opaque colour. */
Color yuvToRgb(int luma, int blueDifference, int redDifference);

/**
 * Decode packed YUV 4:2:2 into @p image. The image must already be sized.
 *
 * @param uyvy Whether the byte order is UYVY rather than YUY2.
 * @return Nothing, or an `ErrorCode::Truncated` error.
 */
std::expected<void, Error> decodeYuv422(Bytes data, bool uyvy, Image &image);

/** Block-compressed formats made of 4x4 texel blocks. */
enum class BlockFormat {
    Bc1,      /*!< BC1 (DXT1) without punch-through alpha. */
    Bc1Alpha, /*!< BC1 (DXT1) with punch-through alpha. */
    Bc2,      /*!< BC2 (DXT2 and DXT3). */
    Bc3,      /*!< BC3 (DXT4 and DXT5). */
    Bc4,      /*!< BC4, one channel. */
    Bc5,      /*!< BC5, two channels. */
};

/**
 * Decode an image of 4x4 blocks in row-major block order into @p image. The image must already
 * be sized.
 *
 * @return Nothing, or an `ErrorCode::Truncated` error.
 */
std::expected<void, Error> decodeBlocks(Bytes data, BlockFormat format, Image &image);

/**
 * Decode PVRTC 1 into @p image. The image must already be sized.
 *
 * @param twoBitsPerPixel Whether the data is 2 bpp rather than 4 bpp.
 * @return Nothing, or an `ErrorCode::Unsupported` (dimensions) or `ErrorCode::Truncated` error.
 */
std::expected<void, Error> decodePvrtc(Bytes data, bool twoBitsPerPixel, Image &image);

/** Whether @p data has a PowerVR legacy header. */
bool looksLikeLegacy(Bytes data);
/** Whether @p data could be headerless PVRTC from `texturetool`. */
bool looksLikeApplePvrtc(Bytes data);
/** Whether @p data starts with a `PVRT` chunk, optionally after a `GBIX` chunk. */
bool looksLikeDreamcast(Bytes data);
/** Whether @p data is a Dreamcast texture pack. */
bool looksLikeDreamcastPack(Bytes data);

/** Decode a PowerVR legacy texture. */
std::expected<DecodeResult, Error> decodeLegacy(Bytes data, const Options &options);
/** Decode a PowerVR version 3 texture. */
std::expected<DecodeResult, Error> decodeVersion3(Bytes data, const Options &options);
/** Decode headerless PVRTC. */
std::expected<DecodeResult, Error> decodeApplePvrtc(Bytes data, const Options &options);
/** Decode a Dreamcast texture. */
std::expected<DecodeResult, Error> decodeDreamcast(Bytes data, const Options &options);
/** Decode a Dreamcast texture pack. */
std::expected<DecodeResult, Error> decodeDreamcastPack(Bytes data, const Options &options);

} // namespace Pvr::Detail
