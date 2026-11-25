#include "pvrthumbnailcreator.h"

#include <KPluginFactory>
#include <QFile>
#include <QImage>

#include "pvrimage.h"

K_PLUGIN_CLASS_WITH_JSON(PvrThumbnailCreator, "pvrthumbnail.json")

PvrThumbnailCreator::PvrThumbnailCreator(QObject *parent, const QVariantList &args)
    : KIO::ThumbnailCreator(parent, args) {
}

KIO::ThumbnailResult PvrThumbnailCreator::create(const KIO::ThumbnailRequest &request) {
    QFile file(request.url().toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return KIO::ThumbnailResult::fail();
    }
    // KIO selects the thumbnailer by MIME type after matching *.pvr or the magic. Headerless PVRTC
    // is therefore accepted.
    auto image = QuickPvr::decodeImage(file.readAll(), true);
    if (!image) {
        return KIO::ThumbnailResult::fail();
    }
    const auto size = image->size().scaled(request.targetSize(), Qt::KeepAspectRatio);
    // Small textures are enlarged without smoothing so their texels remain sharp.
    const auto mode =
        size.width() > image->width() ? Qt::FastTransformation : Qt::SmoothTransformation;
    return KIO::ThumbnailResult::pass(image->scaled(size, Qt::KeepAspectRatio, mode));
}

#include "pvrthumbnailcreator.moc"
