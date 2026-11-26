#include "pvrextractor.h"

#include <cstdint>
#include <span>
#include <string>

#include <KFileMetaData/ExtractionResult>
#include <KFileMetaData/Properties>
#include <QFile>

#include "pvr/pvr.h"

using KFileMetaData::ExtractionResult;
namespace Property = KFileMetaData::Property;

namespace {

// Files larger than 256 MiB are not read.
constexpr qint64 kMaxFileSize = 256LL * 1024 * 1024;

QString fromUtf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

PvrExtractor::PvrExtractor(QObject *parent) : ExtractorPlugin(parent) {
}

QStringList PvrExtractor::mimetypes() const {
    return {QStringLiteral("image/x-pvr")};
}

void PvrExtractor::extract(ExtractionResult *result) {
    result->addType(KFileMetaData::Type::Image);
    if (!(result->inputFlags() & ExtractionResult::ExtractMetaData)) {
        return;
    }
    QFile file(result->inputUrl());
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxFileSize) {
        return;
    }
    const auto contents = file.readAll();
    const auto decoded =
        Pvr::decode(std::span(reinterpret_cast<const std::uint8_t *>(contents.constData()),
                              static_cast<std::size_t>(contents.size())));
    if (!decoded) {
        return;
    }
    const auto details = Pvr::describe(*decoded);
    auto description = std::string(Pvr::containerName(details.container));
    if (details.container == Pvr::Container::DreamcastPack) {
        description += ", " + std::to_string(details.textureCount) + " textures";
    }
    result->add(Property::Description, fromUtf8(description));
    if (details.width && details.height) {
        result->add(Property::Width, *details.width);
        result->add(Property::Height, *details.height);
    }
    auto format = details.format;
    if (details.bitsPerPixel.value_or(0) != 0) {
        format += ", " + std::to_string(*details.bitsPerPixel) + " bpp";
    }
    if (!format.empty()) {
        result->add(Property::PixelFormat, fromUtf8(format));
    }
}
