#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "pvr/pvr.h"

namespace {

using Bytes = std::vector<std::uint8_t>;

struct TestCase {
    std::string_view name;
    std::function<void()> body;
};

std::vector<TestCase> &registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registration {
    Registration(std::string_view name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

// Set by a failing check; the check then returns from the test.
std::string currentFailure;

bool recordFailure(const char *file, int line, const std::string &message) {
    std::ostringstream out;
    out << file << ':' << line << ": " << message;
    currentFailure = out.str();
    return false;
}

std::string describe(const Pvr::Color &color) {
    std::ostringstream out;
    out << '(' << +color[0] << ',' << +color[1] << ',' << +color[2] << ',' << +color[3] << ')';
    return out.str();
}

Pvr::Color pixelAt(const Pvr::Image &image, std::uint32_t x, std::uint32_t y) {
    const auto *p = image.rgba.data() + (static_cast<std::size_t>(y) * image.width + x) * 4;
    return {p[0], p[1], p[2], p[3]};
}

#define TEST(name)                                                                                 \
    void name();                                                                                   \
    const Registration register##name(#name, name);                                                \
    void name()

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            recordFailure(__FILE__, __LINE__, "CHECK(" #condition ") failed");                     \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define CHECK_EQ(actual, expected)                                                                 \
    do {                                                                                           \
        const auto actualValue = (actual);                                                         \
        const auto expectedValue = (expected);                                                     \
        if (!(actualValue == expectedValue)) {                                                     \
            std::ostringstream message;                                                            \
            message << #actual " == " #expected " failed: " << +actualValue                        \
                    << " != " << +expectedValue;                                                   \
            recordFailure(__FILE__, __LINE__, message.str());                                      \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define CHECK_PIXEL(image, x, y, ...)                                                              \
    do {                                                                                           \
        const Pvr::Color expectedColor = __VA_ARGS__;                                              \
        const auto actualColor = pixelAt(image, x, y);                                             \
        if (actualColor != expectedColor) {                                                        \
            std::ostringstream message;                                                            \
            message << "pixel (" << (x) << ',' << (y) << ") is " << describe(actualColor)          \
                    << ", expected " << describe(expectedColor);                                   \
            recordFailure(__FILE__, __LINE__, message.str());                                      \
            return;                                                                                \
        }                                                                                          \
    } while (0)

// Unwraps a std::expected, failing the test with the error message when it has an error.
#define REQUIRE_VALUE(variable, expression)                                                        \
    auto variable = (expression);                                                                  \
    if (!variable) {                                                                               \
        recordFailure(__FILE__, __LINE__, #expression " failed: " + variable.error().message);     \
        return;                                                                                    \
    }

void appendU16(Bytes &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void appendU32(Bytes &out, std::uint32_t value) {
    appendU16(out, value & 0xFFFF);
    appendU16(out, value >> 16);
}

void appendU64(Bytes &out, std::uint64_t value) {
    appendU32(out, static_cast<std::uint32_t>(value));
    appendU32(out, static_cast<std::uint32_t>(value >> 32));
}

void appendText(Bytes &out, std::string_view text) {
    out.insert(out.end(), text.begin(), text.end());
}

Bytes readSample(std::string_view name) {
    std::ifstream file(std::string(PVR_SAMPLES_DIR) + "/" + std::string(name), std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(file), {});
}

// A PowerVR version 2 header as PVRTexTool writes it.
Bytes legacyHeader(std::uint32_t width,
                   std::uint32_t height,
                   std::uint32_t flags,
                   std::uint32_t dataSize,
                   std::uint32_t bitCount,
                   std::initializer_list<std::uint32_t> masks) {
    constexpr std::uint32_t kHeaderSize = 52;
    constexpr std::uint32_t kTag = 0x21525650;
    Bytes out;
    for (const auto value : {kHeaderSize, height, width, 0U, flags, dataSize, bitCount}) {
        appendU32(out, value);
    }
    for (const auto mask : masks) {
        appendU32(out, mask);
    }
    appendU32(out, kTag);
    appendU32(out, 1);
    return out;
}

Bytes pvrtChunk(std::uint8_t pixelFormat,
                std::uint8_t dataFormat,
                std::uint16_t width,
                std::uint16_t height,
                const Bytes &body) {
    Bytes out;
    appendText(out, "PVRT");
    appendU32(out, static_cast<std::uint32_t>(8 + body.size()));
    out.push_back(pixelFormat);
    out.push_back(dataFormat);
    appendU16(out, 0);
    appendU16(out, width);
    appendU16(out, height);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

// Twiddled offset of (x, y) in a square texture, with y in the even bits and x in the odd bits.
std::size_t morton(std::uint32_t x, std::uint32_t y) {
    std::size_t index = 0;
    for (unsigned bit = 0; bit < 16; ++bit) {
        index |= static_cast<std::size_t>((y >> bit) & 1) << (2 * bit);
        index |= static_cast<std::size_t>((x >> bit) & 1) << (2 * bit + 1);
    }
    return index;
}

Bytes version3Header(std::uint64_t pixelFormat,
                     std::uint32_t width,
                     std::uint32_t height,
                     const Bytes &metadata = {}) {
    Bytes out;
    appendText(out, std::string_view("PVR\x03", 4));
    appendU32(out, 0);
    appendU64(out, pixelFormat);
    appendU32(out, 0);
    appendU32(out, 0);
    appendU32(out, height);
    appendU32(out, width);
    for (const auto value : {1U, 1U, 1U, 1U}) {
        appendU32(out, value);
    }
    appendU32(out, static_cast<std::uint32_t>(metadata.size()));
    out.insert(out.end(), metadata.begin(), metadata.end());
    return out;
}

std::uint64_t version3Format(std::string_view channels, std::initializer_list<unsigned> bits) {
    std::uint64_t format = 0;
    for (std::size_t i = 0; i < channels.size(); ++i) {
        format |= static_cast<std::uint64_t>(static_cast<unsigned char>(channels[i])) << (8 * i);
    }
    unsigned i = 0;
    for (const auto count : bits) {
        format |= static_cast<std::uint64_t>(count) << (32 + 8 * i);
        ++i;
    }
    return format;
}

double meanDifference(const Pvr::Image &image, const Pvr::Image &reference, bool flipped) {
    const auto row = static_cast<std::size_t>(image.width) * 4;
    double total = 0;
    for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
        const auto y = i / row;
        const auto j = flipped ? (image.height - 1 - y) * row + i % row : i;
        for (std::size_t c = 0; c < 3; ++c) {
            total += std::abs(int{image.rgba[i + c]} - int{reference.rgba[j + c]});
        }
    }
    return total / (image.width * image.height * 3.0);
}

// PowerVR legacy.

TEST(legacyDdrBannerRgba4444) {
    // A DDR S+ banner in RGBA4444 with the alpha flag and masks, filled with 0xD4FF.
    auto data = legacyHeader(4, 2, 0x8010, 16, 16, {0xF000, 0x0F00, 0x00F0, 0x000F});
    for (int i = 0; i < 8; ++i) {
        appendU16(data, 0xD4FF);
    }
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK(result->container == Pvr::Container::Legacy);
    CHECK_EQ(result->images.size(), std::size_t{1});
    const auto &image = result->images[0];
    CHECK_EQ(image.width, 4U);
    CHECK_EQ(image.height, 2U);
    CHECK(image.format == "RGBA4444");
    CHECK_PIXEL(image, 3, 1, {0xDD, 0x44, 0xFF, 0xFF});
}

TEST(legacyUnknownTypeUsesMasks) {
    // Pixel type 0x30 is not in the table. The header masks (BGR565) describe the data.
    auto data = legacyHeader(1, 1, 0x30, 2, 16, {0x001F, 0x07E0, 0xF800, 0});
    appendU16(data, 0x001F);
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK_PIXEL(result->images[0], 0, 0, {0xFF, 0, 0, 0xFF});
}

TEST(legacyVerticalFlipFlag) {
    auto data = legacyHeader(1, 2, 0x10000 | 0x12, 8, 32, {0, 0, 0, 0});
    for (const std::uint8_t value : {1, 2, 3, 4, 5, 6, 7, 8}) {
        data.push_back(value);
    }
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK_PIXEL(result->images[0], 0, 0, {5, 6, 7, 8});
    CHECK_PIXEL(result->images[0], 0, 1, {1, 2, 3, 4});
}

TEST(legacyTwiddled) {
    // 2x2 RGBA8888 twiddled, stored as (0, 0), (0, 1), (1, 0), then (1, 1).
    auto data = legacyHeader(2, 2, 0x200 | 0x12, 16, 32, {0, 0, 0, 0});
    for (const std::uint8_t value : {0, 1, 2, 3}) {
        data.insert(data.end(), 4, value);
    }
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK_PIXEL(result->images[0], 0, 1, {1, 1, 1, 1});
    CHECK_PIXEL(result->images[0], 1, 0, {2, 2, 2, 2});
}

TEST(legacyDxt1) {
    // One BC1 block in which colour 0 is red, colour 1 is blue, and every index selects colour 1.
    auto data = legacyHeader(4, 4, 0x20, 8, 4, {0, 0, 0, 0});
    appendU16(data, 0xF800);
    appendU16(data, 0x001F);
    appendU32(data, 0x55555555);
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK(result->images[0].format == "DXT1");
    CHECK_PIXEL(result->images[0], 2, 2, {0, 0, 0xFF, 0xFF});
}

TEST(legacySamples) {
    REQUIRE_VALUE(reference, Pvr::decode(readSample("pvr2/RGBA8888.pvr")));
    const auto &referenceImage = reference->images[0];
    // The Limbic samples with headers are stored bottom-up without the flip flag, as OpenGL
    // expects, while texturetool's headerless output is top-down.
    for (const auto name : {"pvr2/RGB888.pvr",
                            "pvr2/RGB565.pvr",
                            "pvr2/RGBA4444.pvr",
                            "pvr2/RGBA5551.pvr",
                            "pvr2/pvrtc4bpp.pvr",
                            "pvr2/apple_4bpp.pvr"}) {
        REQUIRE_VALUE(result, Pvr::decode(readSample(name)));
        const auto &image = result->images[0];
        CHECK_EQ(image.width, referenceImage.width);
        CHECK_EQ(image.height, referenceImage.height);
        const bool flipped = std::strstr(name, "apple") != nullptr;
        const auto difference = meanDifference(image, referenceImage, flipped);
        if (difference > 12) {
            recordFailure(__FILE__,
                          __LINE__,
                          std::string(name) + " differs from RGBA8888 by " +
                              std::to_string(difference));
            return;
        }
    }
    REQUIRE_VALUE(banner, Pvr::decode(readSample("brighterday.pvr")));
    CHECK_EQ(banner->images[0].width, 256U);
    CHECK_EQ(banner->images[0].height, 64U);
}

TEST(applePvrtcDetection) {
    REQUIRE_VALUE(twoBit, Pvr::decode(readSample("pvr2/apple_2bpp.pvr")));
    CHECK(twoBit->container == Pvr::Container::ApplePvrtc);
    CHECK(twoBit->images[0].format == "PVRTC 2bpp");
    CHECK_EQ(twoBit->images[0].width, 256U);
    REQUIRE_VALUE(fourBit, Pvr::decode(readSample("pvr2/apple_4bpp.pvr")));
    CHECK(fourBit->images[0].format == "PVRTC 4bpp");
    CHECK_EQ(fourBit->images[0].width, 256U);
}

// PowerVR version 3.

TEST(version3Rgba8888) {
    auto data = version3Header(version3Format("rgba", {8, 8, 8, 8}), 2, 1);
    for (const std::uint8_t value : {10, 20, 30, 40, 50, 60, 70, 80}) {
        data.push_back(value);
    }
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK(result->container == Pvr::Container::Version3);
    CHECK(result->images[0].format == "RGBA8888");
    CHECK_PIXEL(result->images[0], 1, 0, {50, 60, 70, 80});
}

TEST(version3PackedRgb565) {
    // Packed formats put the first channel in the most significant bits.
    auto data = version3Header(version3Format("rgb", {5, 6, 5}), 1, 1);
    appendU16(data, 0x07E0);
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK_PIXEL(result->images[0], 0, 0, {0, 0xFF, 0, 0xFF});
}

TEST(version3LuminanceAlpha) {
    auto data = version3Header(version3Format("la", {8, 8}), 1, 1);
    data.push_back(0x80);
    data.push_back(0x40);
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK_PIXEL(result->images[0], 0, 0, {0x80, 0x80, 0x80, 0x40});
}

TEST(version3OrientationMetadata) {
    Bytes metadata;
    appendU32(metadata, 0x03525650);
    appendU32(metadata, 3);
    appendU32(metadata, 3);
    // A Y orientation of 1 means the rows run bottom-up.
    metadata.insert(metadata.end(), {0, 1, 0});
    auto data = version3Header(version3Format("rgba", {8, 8, 8, 8}), 1, 2, metadata);
    for (const std::uint8_t value : {1, 1, 1, 1, 2, 2, 2, 2}) {
        data.push_back(value);
    }
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK_PIXEL(result->images[0], 0, 0, {2, 2, 2, 2});
}

TEST(version3Bc3) {
    constexpr std::uint64_t kBc3 = 11;
    auto data = version3Header(kBc3, 4, 4);
    // Alpha endpoints 255 and 0 with every index 1 (alpha 0); the colour block is green.
    data.insert(data.end(), {0xFF, 0x00, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24});
    appendU16(data, 0x07E0);
    appendU16(data, 0x0000);
    appendU32(data, 0);
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK_PIXEL(result->images[0], 1, 1, {0, 0xFF, 0, 0});
}

TEST(version3UnsupportedFormat) {
    constexpr std::uint64_t kEtc1 = 6;
    auto data = version3Header(kEtc1, 4, 4);
    data.resize(data.size() + 8);
    const auto result = Pvr::decode(data);
    CHECK(!result);
    CHECK(result.error().code == Pvr::ErrorCode::Unsupported);
}

// Dreamcast.

TEST(dreamcastTwiddledWithGlobalIndex) {
    // 2x2 RGB565 in storage order: (0, 0) red, (0, 1) green, (1, 0) blue, then (1, 1) white.
    Bytes body;
    for (const auto value : {0xF800U, 0x07E0U, 0x001FU, 0xFFFFU}) {
        appendU16(body, value);
    }
    Bytes data;
    appendText(data, "GBIX");
    appendU32(data, 8);
    appendU64(data, 1234);
    const auto chunk = pvrtChunk(0x01, 0x01, 2, 2, body);
    data.insert(data.end(), chunk.begin(), chunk.end());
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK(result->container == Pvr::Container::Dreamcast);
    const auto &image = result->images[0];
    CHECK(image.format == "RGB565, twiddled, global index 1234");
    CHECK_PIXEL(image, 0, 0, {0xFF, 0, 0, 0xFF});
    CHECK_PIXEL(image, 0, 1, {0, 0xFF, 0, 0xFF});
    CHECK_PIXEL(image, 1, 0, {0, 0, 0xFF, 0xFF});
    CHECK_PIXEL(image, 1, 1, {0xFF, 0xFF, 0xFF, 0xFF});
}

TEST(dreamcastArgbFormats) {
    Bytes body;
    // ARGB1555 red with the alpha bit clear.
    appendU16(body, 0x7C00);
    REQUIRE_VALUE(transparent, Pvr::decode(pvrtChunk(0x00, 0x09, 1, 1, body)));
    CHECK_PIXEL(transparent->images[0], 0, 0, {0xFF, 0, 0, 0});
    body.clear();
    // Opaque ARGB4444 green.
    appendU16(body, 0xF0F0);
    REQUIRE_VALUE(opaque, Pvr::decode(pvrtChunk(0x02, 0x09, 1, 1, body)));
    CHECK_PIXEL(opaque->images[0], 0, 0, {0, 0xFF, 0, 0xFF});
}

TEST(dreamcastTwiddledRectangle) {
    // 4x2 as two 2x2 twiddled blocks side by side. Each texel's red field is x + 4y.
    Bytes body(16);
    for (std::uint32_t y = 0; y < 2; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            const auto index = (x / 2) * 4 + morton(x % 2, y);
            const auto value = static_cast<std::uint16_t>((x + 4 * y) << 11);
            body[index * 2] = static_cast<std::uint8_t>(value);
            body[index * 2 + 1] = static_cast<std::uint8_t>(value >> 8);
        }
    }
    REQUIRE_VALUE(result, Pvr::decode(pvrtChunk(0x01, 0x0D, 4, 2, body)));
    for (std::uint32_t y = 0; y < 2; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            const auto field = x + 4 * y;
            const auto red = static_cast<std::uint8_t>((field << 3) | (field >> 2));
            CHECK_PIXEL(result->images[0], x, y, {red, 0, 0, 0xFF});
        }
    }
}

TEST(dreamcastMipmapsUseLastLevel) {
    // 2x2 with mipmaps: 2 bytes of padding, the 1x1 level (blue), then the 2x2 level (red).
    Bytes body;
    appendU16(body, 0);
    appendU16(body, 0x001F);
    for (int i = 0; i < 4; ++i) {
        appendU16(body, 0xF800);
    }
    REQUIRE_VALUE(result, Pvr::decode(pvrtChunk(0x01, 0x02, 2, 2, body)));
    CHECK_PIXEL(result->images[0], 1, 1, {0xFF, 0, 0, 0xFF});
}

TEST(dreamcastVectorQuantized) {
    // 4x4 with a 256-entry codebook. Entry 1 is red, green, blue, and white in twiddled order.
    Bytes body(256 * 8);
    const std::uint16_t entry[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    for (int i = 0; i < 4; ++i) {
        body[8 + i * 2] = static_cast<std::uint8_t>(entry[i]);
        body[8 + i * 2 + 1] = static_cast<std::uint8_t>(entry[i] >> 8);
    }
    // Block (0, 0) uses entry 1 and the other blocks entry 0.
    body.insert(body.end(), {1, 0, 0, 0});
    REQUIRE_VALUE(result, Pvr::decode(pvrtChunk(0x01, 0x03, 4, 4, body)));
    const auto &image = result->images[0];
    CHECK(image.format == "RGB565, VQ");
    CHECK_PIXEL(image, 0, 0, {0xFF, 0, 0, 0xFF});
    CHECK_PIXEL(image, 0, 1, {0, 0xFF, 0, 0xFF});
    CHECK_PIXEL(image, 1, 0, {0, 0, 0xFF, 0xFF});
    CHECK_PIXEL(image, 1, 1, {0xFF, 0xFF, 0xFF, 0xFF});
    CHECK_PIXEL(image, 3, 3, {0, 0, 0, 0xFF});
}

TEST(dreamcastPalettized) {
    // 2x2 at 4 bpp: indices 0, 1, 2, and 3 in twiddled order, low nibble first.
    const Bytes body = {0x10, 0x32};
    REQUIRE_VALUE(gray, Pvr::decode(pvrtChunk(0x02, 0x05, 2, 2, body)));
    CHECK(gray->images[0].format.find("no palette") != std::string::npos);
    // Index 3 of 16.
    CHECK_PIXEL(gray->images[0], 1, 1, {51, 51, 51, 0xFF});

    Bytes palette;
    appendText(palette, "PVPL");
    appendU32(palette, 8 + 4 * 2);
    palette.insert(palette.end(), {0x02, 0x00});
    appendU16(palette, 0);
    appendU16(palette, 0);
    appendU16(palette, 4);
    for (const auto value : {0xF000U, 0xFF00U, 0xF0F0U, 0xF00FU}) {
        appendU16(palette, value);
    }
    REQUIRE_VALUE(colors, Pvr::decodePalette(palette));
    CHECK_EQ(colors->size(), std::size_t{4});
    Pvr::Options options;
    options.palette = *colors;
    REQUIRE_VALUE(result, Pvr::decode(pvrtChunk(0x02, 0x05, 2, 2, body), options));
    CHECK_PIXEL(result->images[0], 0, 1, {0xFF, 0, 0, 0xFF});
    CHECK_PIXEL(result->images[0], 1, 1, {0, 0, 0xFF, 0xFF});
}

TEST(dreamcastYuv422) {
    // Y = 128 with neutral chroma is mid grey.
    Bytes body;
    appendU16(body, 0x8080);
    appendU16(body, 0x8080);
    REQUIRE_VALUE(result, Pvr::decode(pvrtChunk(0x03, 0x09, 2, 1, body)));
    CHECK_PIXEL(result->images[0], 1, 0, {128, 128, 128, 0xFF});
}

TEST(dreamcastPackWithTruncatedLastEntry) {
    Bytes body;
    for (int i = 0; i < 4; ++i) {
        appendU16(body, 0x07E0);
    }
    const auto first = pvrtChunk(0x01, 0x01, 2, 2, body);
    const auto second = pvrtChunk(0x01, 0x09, 2, 2, body);
    Bytes data;
    // Three table slots: two entries and the zero terminator.
    appendU32(data, 24);
    appendU32(data, static_cast<std::uint32_t>(first.size()));
    appendU32(data, static_cast<std::uint32_t>(24 + first.size()));
    // The last size is overstated by 8 bytes, as in Incoming.
    appendU32(data, static_cast<std::uint32_t>(second.size() + 8));
    appendU32(data, 0);
    appendU32(data, 0);
    data.insert(data.end(), first.begin(), first.end());
    data.insert(data.end(), second.begin(), second.end());
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK(result->container == Pvr::Container::DreamcastPack);
    CHECK_EQ(result->images.size(), std::size_t{2});
    CHECK(result->failures.empty());
    CHECK(result->images[1].format.starts_with("texture 1:"));
    CHECK_PIXEL(result->images[1], 1, 1, {0, 0xFF, 0, 0xFF});

    REQUIRE_VALUE(sheet, Pvr::contactSheet(result->images, 4));
    CHECK_EQ(sheet->width, 2U + 4U + 2U);
    CHECK_EQ(sheet->height, 2U);
}

TEST(dreamcastPackReportsBadEntries) {
    const auto good = pvrtChunk(0x01, 0x01, 2, 2, Bytes(8));
    Bytes data;
    appendU32(data, 16);
    appendU32(data, static_cast<std::uint32_t>(good.size()));
    // An offset outside the pack.
    appendU32(data, 0xFFFFFF);
    appendU32(data, 4);
    data.insert(data.end(), good.begin(), good.end());
    REQUIRE_VALUE(result, Pvr::decode(data));
    CHECK_EQ(result->images.size(), std::size_t{1});
    CHECK_EQ(result->failures.size(), std::size_t{1});
    CHECK_EQ(result->failures[0].position, std::size_t{1});
}

// Robustness.

TEST(rejectsGarbageAndTruncation) {
    const Bytes garbage = {'n', 'o', 'p', 'e', 0, 1, 2};
    CHECK(!Pvr::detect(garbage));
    const auto unrecognized = Pvr::decode(garbage);
    CHECK(!unrecognized);
    CHECK(unrecognized.error().code == Pvr::ErrorCode::Unrecognized);
    CHECK(!Pvr::decode(Bytes{}));

    auto truncated = legacyHeader(64, 64, 0x12, 64 * 64 * 4, 32, {0, 0, 0, 0});
    truncated.resize(truncated.size() + 100);
    const auto shortData = Pvr::decode(truncated);
    CHECK(!shortData);
    CHECK(shortData.error().code == Pvr::ErrorCode::Truncated);

    const Bytes body(8);
    CHECK(!Pvr::decode(pvrtChunk(0x01, 0x01, 64, 64, body)));
    const auto unknownPixel = Pvr::decode(pvrtChunk(0x07, 0x01, 2, 2, body));
    CHECK(!unknownPixel);
    CHECK(unknownPixel.error().code == Pvr::ErrorCode::Unsupported);
    CHECK(!Pvr::decode(pvrtChunk(0x01, 0x7F, 2, 2, body)));
    // Twiddled textures need power-of-two dimensions.
    CHECK(!Pvr::decode(pvrtChunk(0x01, 0x01, 3, 3, Bytes(18))));
}

TEST(enforcesSizeLimits) {
    const auto data = legacyHeader(4096, 4096, 0x12, 4096U * 4096 * 4, 32, {0, 0, 0, 0});
    Pvr::Options options;
    options.maxDimension = 1024;
    const auto result = Pvr::decode(data, options);
    CHECK(!result);
    CHECK(result.error().code == Pvr::ErrorCode::TooLarge);
}

TEST(contactSheetRejectsPaddingThatWouldWrap) {
    // Seven images take three columns and three rows. In each dimension, two gaps of 0x80000001
    // wrap to 2 in 32-bit arithmetic.
    const std::vector<Pvr::Image> images(7, Pvr::Image{1, 1, Bytes(4), {}});
    const auto sheet = Pvr::contactSheet(images, 0x80000001U);
    CHECK(!sheet);
    CHECK(sheet.error().code == Pvr::ErrorCode::TooLarge);
}

} // namespace

int main(int argc, char *argv[]) {
    int run = 0;
    int failed = 0;
    for (const auto &test : registry()) {
        if (argc > 1 && test.name != argv[1]) {
            continue;
        }
        ++run;
        currentFailure.clear();
        test.body();
        if (currentFailure.empty()) {
            std::cout << "ok   " << test.name << '\n';
        } else {
            ++failed;
            std::cout << "FAIL " << test.name << "\n  " << currentFailure << '\n';
        }
    }
    std::cout << run - failed << '/' << run << " passed\n";
    return failed != 0 || run == 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
