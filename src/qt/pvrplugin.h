#pragma once

#include <QImageIOPlugin>

/** Qt image format plugin for PowerVR and Sega Dreamcast PVR textures (format `pvr`). */
class PvrPlugin : public QImageIOPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QImageIOHandlerFactoryInterface_iid FILE "pvr.json")

public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = {}) const override;
};
