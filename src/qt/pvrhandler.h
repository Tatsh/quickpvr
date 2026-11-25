#pragma once

#include <QImageIOHandler>

/** Reads PowerVR and Sega Dreamcast PVR textures for `QImageReader`. */
class PvrHandler : public QImageIOHandler {
public:
    bool canRead() const override;
    bool read(QImage *image) override;

    /**
     * Whether the data at the current position of @p device is a PVR texture. The position is not
     * moved.
     *
     * @param device The device to inspect.
     */
    static bool canRead(QIODevice *device);
};
