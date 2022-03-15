#include "clickablelabel.h"
#include "QMouseEvent"
#include "RDUConstants.h"
#include <QPainter>

namespace {
    template <typename T> int sgn(T val) {
        return (T(0) < val) - (val < T(0));
    }
}
ClickableLabel::ClickableLabel(QWidget *parent, Qt::WindowFlags)
    : QLabel(parent)
{
    constexpr int wheelyBuffer = 1000/60;
    startTimer(wheelyBuffer);
}

void ClickableLabel::mousePressEvent(QMouseEvent* event) {
    active = true;
    auto initial = event->pos();
    auto dstSize = this->size();
    double yScale = (double)LINES / dstSize.height();
    double xScale = (double)COLUMNS / dstSize.width();
    initial.rx() *= xScale;
    initial.ry() *= yScale;
    emit touch(initial);
}
void ClickableLabel::mouseMoveEvent(QMouseEvent* event) {
    if(active) {
        auto initial = event->pos();
        auto dstSize = this->size();
        double yScale = (double)LINES / dstSize.width();
        double xScale = (double)COLUMNS / dstSize.width();
        initial.rx() *= xScale;
        initial.ry() *= yScale;
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
    QPainter painter(this);
    painter.drawImage(0,0,toRender);
}
