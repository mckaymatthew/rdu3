#ifndef RENDERLABEL_H
#define RENDERLABEL_H

#include <QLabel>
#include <QPoint>
#include <QWheelEvent>
#include <QTimerEvent>
#include <QPaintEvent>
#include <QImage>
#include <QElapsedTimer>

class RenderLabel : public QLabel
{
    Q_OBJECT
public:
    explicit RenderLabel(QWidget *parent=nullptr, Qt::WindowFlags f=Qt::WindowFlags());
    QImage toRender;
    QByteArray* imageBacking = nullptr;
public slots:
    void showStats(bool stats);
    void drawError_n(bool ready);
signals:
    void touch(QPoint l);
    void release();
    void wheely(int clicks);
    void resized(QSize s);
    void wheeld(QWheelEvent *event);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    bool active = false;
    bool noSignal = true;

    QElapsedTimer renderTime;
    QElapsedTimer fpsTime;
    QElapsedTimer frameToFrameTime;
    double previousFPS = 0;
    double previousRender = 0;
    QSize newSize;
    QTransform mat;
    bool stats = false;

};

#endif // RENDERLABEL_H
