#include "qtpvr.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    qtpvr w(0, argc <= 2 ? argv[1] : "");
    w.show();

    return app.exec();
}
