#include "pvrplugin.h"

#include "pvrhandler.h"

QImageIOPlugin::Capabilities PvrPlugin::capabilities(QIODevice *device,
                                                     const QByteArray &format) const {
    if (format == "pvr") {
        return CanRead;
    }
    if (!format.isEmpty() || device == nullptr || !device->isOpen()) {
        return {};
    }
    return PvrHandler::canRead(device) ? CanRead : Capabilities{};
}

QImageIOHandler *PvrPlugin::create(QIODevice *device, const QByteArray &format) const {
    auto *handler = new PvrHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}
