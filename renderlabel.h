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
    bool stats = false;
signals:
    void touch(QPoint l);
    void release();
    void wheely(int clicks);
    void resized(QSize s);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    bool active = false;
    int32_t accumulatedWheelies = 0;
    QElapsedTimer renderTime;
    QElapsedTimer fpsTime;
    QElapsedTimer frameToFrameTime;
    double previousFPS = 0;
    double previousRender = 0;
    QTransform mat;

};

#endif // RENDERLABEL_H