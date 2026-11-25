#include "shellimage.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <utility>

namespace QuickPvrShell {

namespace {

// Larger files are not textures worth previewing and would only use memory.
constexpr std::uint64_t kMaxStreamSize = 256ULL * 1024 * 1024;
constexpr ULONG kReadChunkSize = 1024 * 1024;
constexpr std::size_t kChannelCount = 4;

std::wstring widen(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const auto length =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), length);
    return out;
}

} // namespace

std::expected<std::vector<std::uint8_t>, HRESULT> readStream(IStream *stream) {
    if (stream == nullptr) {
        return std::unexpected(E_POINTER);
    }
    std::vector<std::uint8_t> data;
    while (true) {
        const auto offset = data.size();
        if (offset + kReadChunkSize > kMaxStreamSize) {
            return std::unexpected(E_OUTOFMEMORY);
        }
        data.resize(offset + kReadChunkSize);
        ULONG read = 0;
        const auto result = stream->Read(data.data() + offset, kReadChunkSize, &read);
        data.resize(offset + read);
        if (FAILED(result)) {
            return std::unexpected(result);
        }
        if (result == S_FALSE || read == 0) {
            return data;
        }
    }
}

std::expected<DecodedTexture, HRESULT> decodeTexture(const std::vector<std::uint8_t> &data) {
    auto result = Pvr::decode(data);
    if (!result) {
        return std::unexpected(E_FAIL);
    }
    const auto container = std::string(Pvr::containerName(result->container));
    const bool sheet =
        result->images.size() != 1 || result->container == Pvr::Container::DreamcastPack;
    DecodedTexture texture;
    if (sheet) {
        auto image = Pvr::contactSheet(result->images);
        if (!image) {
            return std::unexpected(E_FAIL);
        }
        texture.summary =
            widen(container + " · " + std::to_string(result->images.size()) + " textures");
        texture.image = std::move(*image);
    } else {
        texture.image = std::move(result->images.front());
        texture.summary = widen(container + " · " + std::to_string(texture.image.width) + "×" +
                                std::to_string(texture.image.height) + " " + texture.image.format);
    }
    return texture;
}

Pvr::Image scaleToFit(const Pvr::Image &image, std::uint32_t maxWidth, std::uint32_t maxHeight) {
    const double scale = std::min(static_cast<double>(maxWidth) / image.width,
                                  static_cast<double>(maxHeight) / image.height);
    Pvr::Image out;
    out.width = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(image.width * scale));
    out.height = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(image.height * scale));
    out.format = image.format;
    out.rgba.resize(static_cast<std::size_t>(out.width) * out.height * kChannelCount);
    for (std::uint32_t y = 0; y < out.height; ++y) {
        const auto top =
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) * image.height / out.height);
        const auto bottom = std::max(top + 1,
                                     static_cast<std::uint32_t>(static_cast<std::uint64_t>(y + 1) *
                                                                image.height / out.height));
        for (std::uint32_t x = 0; x < out.width; ++x) {
            const auto left =
                static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) * image.width / out.width);
            const auto right =
                std::max(left + 1,
                         static_cast<std::uint32_t>(static_cast<std::uint64_t>(x + 1) *
                                                    image.width / out.width));
            // Average the source texels the output pixel covers. When enlarging, the pixel covers
            // one texel.
            std::array<std::uint64_t, kChannelCount> sums{};
            for (auto sourceY = top; sourceY < bottom; ++sourceY) {
                for (auto sourceX = left; sourceX < right; ++sourceX) {
                    const auto *pixel =
                        image.rgba.data() +
                        (static_cast<std::size_t>(sourceY) * image.width + sourceX) * kChannelCount;
                    for (std::size_t c = 0; c < kChannelCount; ++c) {
                        sums[c] += pixel[c];
                    }
                }
            }
            const auto count = static_cast<std::uint64_t>(bottom - top) * (right - left);
            auto *pixel =
                out.rgba.data() + (static_cast<std::size_t>(y) * out.width + x) * kChannelCount;
            for (std::size_t c = 0; c < kChannelCount; ++c) {
                pixel[c] = static_cast<std::uint8_t>((sums[c] + count / 2) / count);
            }
        }
    }
    return out;
}

HBITMAP createBitmap(const Pvr::Image &image) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = static_cast<LONG>(image.width);
    // A negative height makes the rows top-down, matching the decoded image.
    info.bmiHeader.biHeight = -static_cast<LONG>(image.height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap == nullptr || bits == nullptr) {
        return nullptr;
    }
    auto *out = static_cast<std::uint8_t *>(bits);
    for (std::size_t i = 0; i < image.rgba.size(); i += kChannelCount) {
        out[i] = image.rgba[i + 2];
        out[i + 1] = image.rgba[i + 1];
        out[i + 2] = image.rgba[i];
        out[i + 3] = image.rgba[i + 3];
    }
    return bitmap;
}

} // namespace QuickPvrShell
