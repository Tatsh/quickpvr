#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "internal.h"
#include "pvrtc.h"

namespace Pvr::Detail {

namespace {

constexpr unsigned kBlockSide = 4;
constexpr unsigned kTexelsPerBlock = kBlockSide * kBlockSide;
constexpr unsigned kChannelCount = 4;
constexpr unsigned kAlphaChannel = 3;
constexpr std::uint8_t kOpaque = 255;
constexpr std::size_t kHalfBlockSize = 8;
constexpr std::size_t kFullBlockSize = 16;
constexpr unsigned kRed565Bits = 5;
constexpr unsigned kGreen565Bits = 6;
constexpr unsigned kBlue565Bits = 5;
constexpr unsigned kExplicitAlphaBits = 4;
constexpr unsigned kChannelIndexBits = 3;
constexpr std::size_t kChannelIndexBytes = 6;
constexpr unsigned kPvrtcTwoBitBlockWidth = 8;
constexpr unsigned kPvrtcFourBitBlockWidth = 4;
constexpr unsigned kPvrtcBlockHeight = 4;
constexpr std::size_t kPvrtcMinimumBlocks = 2;
constexpr std::size_t kPvrtcBlockSize = 8;
constexpr double kSignedUnitScale = 127.5;

// A decoded 4x4 block of 16 RGBA texels in row-major order.
using TexelBlock = std::array<std::uint8_t, kTexelsPerBlock * kChannelCount>;

Color from565(std::uint16_t value) {
    return {expandBits((value >> (kGreen565Bits + kBlue565Bits)) & 0x1F, kRed565Bits),
            expandBits((value >> kBlue565Bits) & 0x3F, kGreen565Bits),
            expandBits(value & 0x1F, kBlue565Bits),
            kOpaque};
}

std::uint8_t mix(unsigned first, unsigned second, unsigned firstWeight, unsigned secondWeight) {
    const auto total = firstWeight + secondWeight;
    return static_cast<std::uint8_t>((first * firstWeight + second * secondWeight + total / 2) /
                                     total);
}

// The colour half of a BC1-style block. Three-colour mode (with a transparent fourth entry)
// applies only to BC1; BC2 and BC3 always interpolate four colours.
void decodeColorBlock(Bytes block, bool threeColorAllowed, TexelBlock &out) {
    const auto first = loadU16(block, 0);
    const auto second = loadU16(block, 2);
    std::array<Color, 4> palette{from565(first), from565(second), Color{}, Color{}};
    if (first > second || !threeColorAllowed) {
        for (unsigned c = 0; c < kAlphaChannel; ++c) {
            palette[2][c] = mix(palette[0][c], palette[1][c], 2, 1);
            palette[3][c] = mix(palette[0][c], palette[1][c], 1, 2);
        }
        palette[2][kAlphaChannel] = kOpaque;
        palette[3][kAlphaChannel] = kOpaque;
    } else {
        for (unsigned c = 0; c < kAlphaChannel; ++c) {
            palette[2][c] = mix(palette[0][c], palette[1][c], 1, 1);
        }
        palette[2][kAlphaChannel] = kOpaque;
        palette[3] = Color{};
    }
    const auto indices = loadU32(block, 4);
    for (unsigned i = 0; i < kTexelsPerBlock; ++i) {
        const auto &color = palette[(indices >> (2 * i)) & 3];
        std::copy(color.begin(), color.end(), out.begin() + i * kChannelCount);
    }
}

// A BC4-style 8-byte block of one channel, written to @p channel of each texel.
void decodeChannelBlock(Bytes block, unsigned channel, TexelBlock &out) {
    const unsigned first = block[0];
    const unsigned second = block[1];
    std::array<std::uint8_t, 8> values{static_cast<std::uint8_t>(first),
                                       static_cast<std::uint8_t>(second)};
    if (first > second) {
        for (unsigned i = 1; i < 7; ++i) {
            values[i + 1] = mix(first, second, 7 - i, i);
        }
    } else {
        for (unsigned i = 1; i < 5; ++i) {
            values[i + 1] = mix(first, second, 5 - i, i);
        }
        values[6] = 0;
        values[7] = kOpaque;
    }
    const auto bits = loadUint(block, 2, kChannelIndexBytes);
    for (unsigned i = 0; i < kTexelsPerBlock; ++i) {
        out[i * kChannelCount + channel] =
            values[(bits >> (kChannelIndexBits * i)) & ((1U << kChannelIndexBits) - 1)];
    }
}

void decodeBlock(Bytes block, BlockFormat format, TexelBlock &out) {
    switch (format) {
    case BlockFormat::Bc1:
    case BlockFormat::Bc1Alpha:
        decodeColorBlock(block, true, out);
        if (format == BlockFormat::Bc1) {
            for (unsigned i = 0; i < kTexelsPerBlock; ++i) {
                out[i * kChannelCount + kAlphaChannel] = kOpaque;
            }
        }
        break;
    case BlockFormat::Bc2:
        decodeColorBlock(block.subspan(kHalfBlockSize), false, out);
        for (unsigned i = 0; i < kTexelsPerBlock; ++i) {
            const auto nibble = (block[i / 2] >> ((i % 2) * kExplicitAlphaBits)) & 0xF;
            out[i * kChannelCount + kAlphaChannel] =
                expandBits(static_cast<std::uint64_t>(nibble), kExplicitAlphaBits);
        }
        break;
    case BlockFormat::Bc3:
        decodeColorBlock(block.subspan(kHalfBlockSize), false, out);
        decodeChannelBlock(block, kAlphaChannel, out);
        break;
    case BlockFormat::Bc4:
        decodeChannelBlock(block, 0, out);
        for (unsigned i = 0; i < kTexelsPerBlock; ++i) {
            out[i * kChannelCount + 1] = out[i * kChannelCount];
            out[i * kChannelCount + 2] = out[i * kChannelCount];
            out[i * kChannelCount + kAlphaChannel] = kOpaque;
        }
        break;
    case BlockFormat::Bc5:
        decodeChannelBlock(block, 0, out);
        decodeChannelBlock(block.subspan(kHalfBlockSize), 1, out);
        for (unsigned i = 0; i < kTexelsPerBlock; ++i) {
            // Reconstruct Z so tangent-space normal maps look like normal maps.
            const double x = out[i * kChannelCount] / kSignedUnitScale - 1;
            const double y = out[i * kChannelCount + 1] / kSignedUnitScale - 1;
            const double z = std::sqrt(std::max(0.0, 1 - x * x - y * y));
            out[i * kChannelCount + 2] =
                static_cast<std::uint8_t>(std::lround((z + 1) * kSignedUnitScale));
            out[i * kChannelCount + kAlphaChannel] = kOpaque;
        }
        break;
    }
}

std::size_t blockSize(BlockFormat format) {
    switch (format) {
    case BlockFormat::Bc1:
    case BlockFormat::Bc1Alpha:
    case BlockFormat::Bc4:
        return kHalfBlockSize;
    case BlockFormat::Bc2:
    case BlockFormat::Bc3:
    case BlockFormat::Bc5:
        return kFullBlockSize;
    }
    return kFullBlockSize;
}

std::size_t pvrtcSize(std::uint32_t width, std::uint32_t height, bool twoBitsPerPixel) {
    const std::size_t blocksX = std::max<std::size_t>(
        kPvrtcMinimumBlocks,
        width / (twoBitsPerPixel ? kPvrtcTwoBitBlockWidth : kPvrtcFourBitBlockWidth));
    const std::size_t blocksY =
        std::max<std::size_t>(kPvrtcMinimumBlocks, height / kPvrtcBlockHeight);
    return blocksX * blocksY * kPvrtcBlockSize;
}

} // namespace

std::expected<void, Error> decodeBlocks(Bytes data, BlockFormat format, Image &image) {
    const auto blocksX = (static_cast<std::size_t>(image.width) + kBlockSide - 1) / kBlockSide;
    const auto blocksY = (static_cast<std::size_t>(image.height) + kBlockSide - 1) / kBlockSide;
    const auto size = blockSize(format);
    const auto body = slice(data, 0, blocksX * blocksY * size, "Block-compressed data");
    if (!body) {
        return std::unexpected(body.error());
    }
    TexelBlock texels{};
    for (std::size_t blockY = 0; blockY < blocksY; ++blockY) {
        for (std::size_t blockX = 0; blockX < blocksX; ++blockX) {
            decodeBlock(body->subspan((blockY * blocksX + blockX) * size, size), format, texels);
            for (std::size_t y = 0; y < kBlockSide && blockY * kBlockSide + y < image.height; ++y) {
                for (std::size_t x = 0; x < kBlockSide && blockX * kBlockSide + x < image.width;
                     ++x) {
                    const auto *texel = texels.data() + (y * kBlockSide + x) * kChannelCount;
                    setPixel(image,
                             static_cast<std::uint32_t>(blockX * kBlockSide + x),
                             static_cast<std::uint32_t>(blockY * kBlockSide + y),
                             {texel[0], texel[1], texel[2], texel[3]});
                }
            }
        }
    }
    return {};
}

std::expected<void, Error> decodePvrtc(Bytes data, bool twoBitsPerPixel, Image &image) {
    if (!isPowerOfTwo(image.width) || !isPowerOfTwo(image.height)) {
        return fail(ErrorCode::Unsupported,
                    "PVRTC textures must have power-of-two dimensions, not " +
                        std::to_string(image.width) + "x" + std::to_string(image.height) + ".");
    }
    const auto body =
        slice(data, 0, pvrtcSize(image.width, image.height, twoBitsPerPixel), "PVRTC data");
    if (!body) {
        return std::unexpected(body.error());
    }
    // Copy into aligned storage because the decoder reads 32-bit words.
    std::vector<AMTC_BLOCK_STRUCT> blocks(body->size() / sizeof(AMTC_BLOCK_STRUCT));
    std::memcpy(blocks.data(), body->data(), body->size());
    Decompress(blocks.data(),
               twoBitsPerPixel ? 1 : 0,
               static_cast<int>(image.width),
               static_cast<int>(image.height),
               1,
               image.rgba.data());
    return {};
}

} // namespace Pvr::Detail
