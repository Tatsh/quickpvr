#include "pvrimage.h"

#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

#include "pvr/pvr.h"

namespace QuickPvr {

namespace {

std::span<const std::uint8_t> bytesOf(QByteArrayView data) {
    return {reinterpret_cast<const std::uint8_t *>(data.data()),
            static_cast<std::size_t>(data.size())};
}

} // namespace

bool isPvrData(QByteArrayView data, bool allowHeaderless) {
    const auto container = Pvr::detect(bytesOf(data));
    return container && (allowHeaderless || *container != Pvr::Container::ApplePvrtc);
}

std::expected<QImage, QString> decodeImage(QByteArrayView data, bool allowHeaderless) {
    if (!isPvrData(data, allowHeaderless)) {
        return std::unexpected(QStringLiteral("Not a recognized PowerVR or Dreamcast texture."));
    }
    auto result = Pvr::decode(bytesOf(data));
    if (!result) {
        return std::unexpected(QString::fromStdString(result.error().message));
    }
    const bool sheet =
        result->images.size() != 1 || result->container == Pvr::Container::DreamcastPack;
    auto image = sheet ? Pvr::contactSheet(result->images) :
                         std::expected<Pvr::Image, Pvr::Error>(std::move(result->images.front()));
    if (!image) {
        return std::unexpected(QString::fromStdString(image.error().message));
    }
    QImage out(
        static_cast<int>(image->width), static_cast<int>(image->height), QImage::Format_RGBA8888);
    if (out.isNull()) {
        return std::unexpected(QStringLiteral("Could not allocate the image."));
    }
    const auto rowSize = static_cast<std::size_t>(image->width) * 4;
    for (std::uint32_t y = 0; y < image->height; ++y) {
        std::memcpy(out.scanLine(static_cast<int>(y)), image->rgba.data() + y * rowSize, rowSize);
    }
    return out;
}

} // namespace QuickPvr
