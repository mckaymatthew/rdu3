#include "renderlabel.h"
#include "QMouseEvent"
#include "RDUConstants.h"
#include <QPainter>

namespace {
    //Take the sign of the value, drop the value.
    template <typename T> int sgn(T val) {
        return (T(0) < val) - (val < T(0));
    }
    //Scale a point (down) from the render size to actual size
    void scale(QPoint& p, QSize area) {
        double yScale = (double)LINES / area.height();
        double xScale = (double)COLUMNS / area.width();
        p.rx() *= xScale;
        p.ry() *= yScale;
    }

}
RenderLabel::RenderLabel(QWidget *parent, Qt::WindowFlags)
    : QLabel(parent)
{
    constexpr int wheelyBuffer = 1000/60; //60 times a second send the wheely updates
    startTimer(wheelyBuffer);
    //Start various timers so they're valid when we go to restart them
    fpsTime.start();
    renderTime.start();
    frameToFrameTime.start();
}

void RenderLabel::mousePressEvent(QMouseEvent* event) {
    active = true; //Record that mouse is held down
    auto initial = event->pos();
    scale(initial, this->size());
    emit touch(initial);
}
void RenderLabel::mouseMoveEvent(QMouseEvent* event) {
    if(active) { //If mouse is held down
        auto initial = event->pos();
        scale(initial, this->size());
        emit touch(initial);
    }
}
void RenderLabel::mouseReleaseEvent(QMouseEvent*) {
    active = false;
    emit release();
}

void RenderLabel::wheelEvent(QWheelEvent* event) {
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

void RenderLabel::timerEvent(QTimerEvent*) {
    if(accumulatedWheelies != 0) {
        emit wheely(accumulatedWheelies);
        accumulatedWheelies = 0;
    }
}

void RenderLabel::resizeEvent(QResizeEvent *event) {
    emit resized(event->size());
    //Calculate new render size.
    QSize newSize(COLUMNS, LINES);
    newSize.scale(size(),Qt::KeepAspectRatio);
    //Calculate the resize transform matrix for later rescaling
    QTransform wm = QTransform::fromScale((qreal)newSize.width() / COLUMNS, (qreal)newSize.height() / LINES);
    mat = QImage::trueMatrix(wm, COLUMNS, LINES);
}

void RenderLabel::paintEvent(QPaintEvent*) {
    renderTime.restart(); //Timer tracking how long paint takes

    //Draw the frame with the resize transformation
    QPainter p(this);
    p.setTransform(mat);
    p.drawImage(QPoint(0, 0), this->toRender);

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
        uh.setY(50);
        p.setFont(QFont("Courier New", 12, QFont::Weight::ExtraBold));

        double renderNetMbps = ((double)g_NetworkBytesPerSecond/g_scaleFactor) * 8.0 / 1024 /1024;
//        double renderNetLps = ((double)g_NetworkLinesPerSecond/g_scaleFactor);
        double renderNetFps = ((double)g_NetworkFramesPerSecond/g_scaleFactor);
        auto stats = QString("Network:       Renderer:"
                "\nFPS: %1 FPS: %3"
                "\nMbps:%2 Draw:%4")
                .arg(renderNetFps,8,'f',2)
                .arg(renderNetMbps,8,'f',2)
                .arg(previousFPS,5,'f',2)
                .arg(previousRender,5,'f',2);


        p.drawText(uh, stats);
    }
}