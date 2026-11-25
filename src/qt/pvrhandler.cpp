#include "pvrhandler.h"

#include <algorithm>
#include <utility>

#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QImage>

#include "pvrimage.h"

namespace {

// Peeking at the whole file lets the container checks see a pack's first entry. Larger files are
// inspected by their first bytes only.
constexpr qint64 kMaxPeekSize = 64 * 1024 * 1024;
constexpr qint64 kSequentialPeekSize = 4096;

// Headerless PVRTC is recognised by its size alone and is accepted only for files named *.pvr.
// Otherwise any file whose size is a power of two would match.
bool allowsHeaderless(const QIODevice *device) {
    const auto *file = qobject_cast<const QFile *>(device);
    return file != nullptr && QFileInfo(file->fileName())
                                      .suffix()
                                      .compare(QStringLiteral("pvr"), Qt::CaseInsensitive) == 0;
}

} // namespace

bool PvrHandler::canRead() const {
    if (!canRead(device())) {
        return false;
    }
    setFormat(QByteArrayLiteral("pvr"));
    return true;
}

bool PvrHandler::canRead(QIODevice *device) {
    if (device == nullptr) {
        return false;
    }
    const auto size = device->isSequential() ?
                          kSequentialPeekSize :
                          std::min(device->size() - device->pos(), kMaxPeekSize);
    return QuickPvr::isPvrData(device->peek(size), allowsHeaderless(device));
}

bool PvrHandler::read(QImage *image) {
    const auto data = device()->readAll();
    auto decoded = QuickPvr::decodeImage(data, allowsHeaderless(device()));
    if (!decoded) {
        qWarning("PvrHandler: %s", qUtf8Printable(decoded.error()));
        return false;
    }
    *image = std::move(*decoded);
    return true;
}
