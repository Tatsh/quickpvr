#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * Decoder for PowerVR and Sega Dreamcast PVR textures.
 *
 * The decoder reads:
 *
 * - PowerVR legacy (version 1 and 2) headers. Uncompressed data uses the documented layout of its
 *   pixel type, or the channel masks in the header for other types. PVRTC 2 and 4 bpp and DXT1 to
 *   DXT5 are decoded too.
 * - PowerVR version 3 headers: uncompressed formats described by channel names and bit counts,
 *   PVRTC 2 and 4 bpp, BC1 to BC5, UYVY, YUY2, RGBG8888, and GRGB8888.
 * - Headerless PVRTC as written by Apple's `texturetool`.
 * - Sega Dreamcast `PVRT` textures with or without a `GBIX` header: ARGB1555, RGB565, ARGB4444,
 *   RGB555, YUV422, and bump-map texels in twiddled, rectangular, stride, VQ, and small VQ layouts,
 *   with or without mipmaps, and 4 and 8 bpp palettised textures.
 * - Dreamcast texture packs (a table of `(offset, size)` pairs pointing at `PVRT` chunks), such as
 *   the `*_T.PVR` files of *Incoming*.
 *
 * Every decoded image is 8-bit RGBA with straight alpha, in top-down row order.
 */
namespace Pvr {

/** Largest width or height accepted by default. */
constexpr std::uint32_t kDefaultMaxDimension = 16384;
/** Largest pixel count accepted by default. */
constexpr std::uint64_t kDefaultMaxPixels = std::uint64_t{1} << 26;
/** Default gap between the cells of a contact sheet, in pixels. */
constexpr std::uint32_t kDefaultSheetPadding = 4;

/** Kind of decoding failure. */
enum class ErrorCode {
    Unrecognized, /*!< The data is not a texture the decoder recognises. */
    Truncated,    /*!< The data ends before the texture does. */
    Invalid,      /*!< The header is inconsistent with itself or with the data. */
    Unsupported,  /*!< The texture uses a format the decoder does not decode. */
    TooLarge,     /*!< The texture exceeds the limits in Options. */
};

/** Failure returned through `std::expected`. */
struct Error {
    ErrorCode code;      /*!< Kind of failure. */
    std::string message; /*!< Sentence describing the failure. */
};

/** Container kinds the decoder recognises. */
enum class Container {
    Legacy,        /*!< PowerVR version 1 or 2 header. */
    Version3,      /*!< PowerVR version 3 header. */
    ApplePvrtc,    /*!< Headerless PVRTC from Apple's `texturetool`. */
    Dreamcast,     /*!< Dreamcast `PVRT` chunk, optionally after a `GBIX` chunk. */
    DreamcastPack, /*!< Table of contents pointing at Dreamcast `PVRT` chunks. */
};

/** One RGBA colour, as a palette entry or a pixel. */
using Color = std::array<std::uint8_t, 4>;

/** A decoded image. */
struct Image {
    std::uint32_t width{};          /*!< Width in pixels. */
    std::uint32_t height{};         /*!< Height in pixels. */
    std::vector<std::uint8_t> rgba; /*!< `width * height * 4` bytes in top-down rows. */
    std::string format;             /*!< Description of the source pixel format. */
};

/** A texture in a pack that could not be decoded. */
struct PackFailure {
    std::size_t position{}; /*!< Index of the entry in the table of contents. */
    Error error;            /*!< Why decoding failed. */
};

/** Everything decoded from one file. */
struct DecodeResult {
    Container container{};             /*!< Container the data was read as. */
    std::vector<Image> images;         /*!< One image, or one per decoded texture of a pack. */
    std::vector<PackFailure> failures; /*!< Pack entries that could not be decoded. */
};

/** Decoder settings. */
struct Options {
    /**
     * Palette for Dreamcast 4 and 8 bpp palettised textures. Without one, indices are shown as a
     * grey ramp.
     */
    std::optional<std::vector<Color>> palette;
    std::uint32_t maxDimension = kDefaultMaxDimension; /*!< Largest accepted width or height. */
    std::uint64_t maxPixels = kDefaultMaxPixels;       /*!< Largest accepted pixel count. */
};

/**
 * Identify the container of a texture without decoding it.
 *
 * @param data The file contents.
 * @return The container, or `std::nullopt` when the data is not recognised.
 */
std::optional<Container> detect(std::span<const std::uint8_t> data);

/**
 * Decode a texture file.
 *
 * A pack in which some entries fail still decodes as long as one entry succeeds. The failed entries
 * are listed in DecodeResult::failures.
 *
 * @param data The file contents.
 * @param options Decoder settings.
 * @return The decoded images, or the reason decoding failed.
 */
std::expected<DecodeResult, Error> decode(std::span<const std::uint8_t> data,
                                          const Options &options = {});

/**
 * Decode a Dreamcast `PVPL` palette, the `.PVP` file accompanying a palettised texture.
 *
 * @param data The palette file contents.
 * @return The palette entries, or an `ErrorCode::Unrecognized` or `ErrorCode::Unsupported` error.
 */
std::expected<std::vector<Color>, Error> decodePalette(std::span<const std::uint8_t> data);

/**
 * Place images on a grid in one picture, for previewing a pack. Each cell is as large as the
 * largest image and each image is centred in its cell. A single image is returned unchanged.
 *
 * @param images The images to place.
 * @param padding Transparent pixels between cells.
 * @param options Limits the sheet must respect.
 * @return The sheet, or an `ErrorCode::Invalid` (no images) or `ErrorCode::TooLarge` error.
 */
std::expected<Image, Error> contactSheet(std::span<const Image> images,
                                         std::uint32_t padding = kDefaultSheetPadding,
                                         const Options &options = {});

/**
 * Short name of a container.
 *
 * @param container The container.
 * @return A name such as `PVR v3`.
 */
std::string_view containerName(Container container);

/**
 * Library version.
 *
 * @return The version, such as `1.0.0`.
 */
std::string_view version();

} // namespace Pvr
