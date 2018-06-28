#ifndef QTPVR_H
#define QTPVR_H

#include <QMainWindow>
#include <QGraphicsScene>

namespace Ui {
class qtpvr;
}

class qtpvr : public QMainWindow
{
    Q_OBJECT

public:
    qtpvr ( QWidget* parent, const char* imagePath = 0 );
    ~qtpvr();

private:
    Ui::qtpvr *ui;
    QGraphicsScene *scene;
    bool renderedImage = false;
};

#endif // QTPVR_H
