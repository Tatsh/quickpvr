#include "qtpvr.h"
#include "ui_qtpvr.h"

#include <QGraphicsScene>
#include <pvr.h>

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
        qDebug("Format: %s", pvr.format);
        qDebug("Should flip: %s", pvr.should_flip ? "true" : "false");

        QImage image(pvr.data, pvr.width, pvr.height, QImage::Format_RGBA8888);
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
