#include "clickablelabel.h"
#include "QMouseEvent"
#include "RDUConstants.h"
#include <QPainter>

namespace {
    template <typename T> int sgn(T val) {
        return (T(0) < val) - (val < T(0));
    }
    void scale(QPoint& p, QSize area) {
        double yScale = (double)LINES / area.height();
        double xScale = (double)COLUMNS / area.width();
        p.rx() *= xScale;
        p.ry() *= yScale;
    }

}
ClickableLabel::ClickableLabel(QWidget *parent, Qt::WindowFlags)
    : QLabel(parent)
{
    constexpr int wheelyBuffer = 1000/60; //60 times a second send the wheely updates
    startTimer(wheelyBuffer);
    fpsTime.start();
    renderTime.start();
}

void ClickableLabel::mousePressEvent(QMouseEvent* event) {
    active = true;
    auto initial = event->pos();
    scale(initial, this->size());
    emit touch(initial);
}
void ClickableLabel::mouseMoveEvent(QMouseEvent* event) {
    if(active) {
        auto initial = event->pos();
        scale(initial, this->size());
        emit touch(initial);
    }
}
void ClickableLabel::mouseReleaseEvent(QMouseEvent*) {
    active = false;
    emit release();
}

void ClickableLabel::wheelEvent(QWheelEvent* event) {
    event->accept();
    QPoint numPixels = event->pixelDelta();
    QPoint numDegrees = event->angleDelta();
    //Which delta depends on platform and input source
    //TODO: Currenly a single wheel event is one tick of the dial
    //Need to figure out how to better map these deltas to multiticks.
    if (!numPixels.isNull()) {
        accumulatedWheelies = accumulatedWheelies + sgn(numPixels.y());
    } else if (!numDegrees.isNull()) {
        accumulatedWheelies = accumulatedWheelies + sgn(numDegrees.y());
    }
}

void ClickableLabel::timerEvent(QTimerEvent*) {
    if(accumulatedWheelies != 0) {
        emit wheely(accumulatedWheelies);
        accumulatedWheelies = 0;
    }
}

void ClickableLabel::paintEvent(QPaintEvent*) {

    renderTime.restart();
    QPainter p(this);
    p.drawImage(0,0,toRender);
    previousRender =
            ((59.0* previousRender) +
            (renderTime.nsecsElapsed() / 1000000)) / 60;
    previousFPS =
            ((59.0* previousFPS) +
            (1000000000.0/fpsTime.nsecsElapsed())) / 60;
    fpsTime.restart();

    if(stats) {
        p.setPen(QPen(Qt::red));
        auto uh = this->geometry();
        uh.setX(0);
        uh.setY(uh.height()-75);
//        QPoint uh(10,10);
//        QRectF uh(0,200,400,400);
        p.setFont(QFont("Courier New", 12, QFont::Weight::ExtraBold));

        double renderNetMbps = ((double)g_NetworkBytesPerSecond/g_scaleFactor) * 8.0 / 1024 /1024;
        double renderNetLps = ((double)g_NetworkLinesPerSecond/g_scaleFactor);
        double renderNetFps = ((double)g_NetworkFramesPerSecond/g_scaleFactor);
        double resize = ((double)g_ResizeTime/g_scaleFactor);
        auto stats = QString("Network:        Renderer:"
                "\n\tFPS:  %1\tFPS:  %4"
                "\n\tLines:%2\tScale:%6"
                "\n\tMbps: %3\tDraw: %5")
                .arg(renderNetFps,8,'f',2)
                .arg(renderNetLps,8,'f',2)
                .arg(renderNetMbps,8,'f',2)
                .arg(previousFPS,8,'f',2)
                .arg(previousRender,8,'f',2)
                .arg(resize,8,'f',2);


        p.drawText(uh, stats);
    }
}
