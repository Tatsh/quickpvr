#pragma once

#include <expected>

#include <QByteArrayView>
#include <QImage>
#include <QString>

namespace QuickPvr {

/**
 * Decode PVR texture data into an image. A texture pack becomes a contact sheet of all its
 * textures.
 *
 * Headerless PVRTC is recognised only by the size of the data. It is decoded only when
 * @p allowHeaderless is set. Callers set it when the file is named `*.pvr`.
 *
 * @param data The file contents.
 * @param allowHeaderless Whether headerless PVRTC may be decoded.
 * @return The image in `QImage::Format_RGBA8888`, or a message describing the failure.
 */
std::expected<QImage, QString> decodeImage(QByteArrayView data, bool allowHeaderless);

/**
 * Whether @p data looks like a PVR texture this library decodes.
 *
 * @param data The start of the file, or all of it.
 * @param allowHeaderless Whether headerless PVRTC is accepted.
 */
bool isPvrData(QByteArrayView data, bool allowHeaderless);

} // namespace QuickPvr
