#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QPoint>
#include <QWheelEvent>
#include <QTimerEvent>
#include <QPaintEvent>
#include <QImage>
#include <QElapsedTimer>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget *parent=nullptr, Qt::WindowFlags f=Qt::WindowFlags());
    QImage toRender;
    bool stats = false;
signals:
    void touch(QPoint l);
    void release();
    void wheely(int clicks);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
private:
    bool active = false;
    int32_t accumulatedWheelies = 0;
    QElapsedTimer renderTime;
    QElapsedTimer fpsTime;
    double previousFPS = 0;
    double previousRender = 0;


};

#endif // CLICKABLELABEL_H
