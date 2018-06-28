#include "qtpvr.h"
#include "ui_qtpvr.h"

#include <QGraphicsScene>
#include <pvr.h>

static QImage::Format getFormatFromString(const char *inFormat) {
    if (!inFormat) {
        return QImage::Format_Invalid;
    }

    QString format(inFormat);
    if (format.isNull()) {
        return QImage::Format_Invalid;
    }

    if (format == "RGBA8888") {
        return QImage::Format_RGBA8888;
    }
    if (format == "RGB555") {
        return QImage::Format_RGB555;
    }
    if (format == "RGB888") {
        return QImage::Format_RGB888;
    }
    if (format == "I8") {
        return QImage::Format_Indexed8;
    }
    if (format == "AI8") {
        return QImage::Format_Alpha8;
    }

    // RGBA4444, RGBA5551, RGB565, PVRTC2, PVRTC4
    return QImage::Format_Invalid;
}

qtpvr::qtpvr(QWidget *parent, const char *imagePath) :
    QMainWindow(parent),
    ui(new Ui::qtpvr),
    scene(new QGraphicsScene(this)) {
    ui->setupUi(this);

    PVRTexture pvr;
    ePVRLoadResult result = pvr.load(imagePath);
    if (result != PVR_LOAD_OKAY) {
        return;
    }

    if (pvr.data) {
        // TODO Add a toolbar with buttons to override these properties because
        // sometimes files don't have the correct flags set
        qDebug("Format: %s", pvr.format);
        qDebug("Should flip: %s", pvr.should_flip ? "true" : "false");

        QImage::Format format = getFormatFromString(pvr.format);

        if (format == QImage::Format_Invalid) {
            return;
        }

        QImage image(pvr.data, pvr.width, pvr.height, format);

        if (!image.isNull()) {
            QPixmap pixmap = QPixmap::fromImage(image);
            scene->addPixmap(pixmap);
            scene->setSceneRect(pixmap.rect());
            ui->graphicsView->setScene(scene);
        }
    }
}

qtpvr::~qtpvr() {
    delete ui;
}
