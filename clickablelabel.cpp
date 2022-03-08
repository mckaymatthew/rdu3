#include "clickablelabel.h"
#include "QMouseEvent"

ClickableLabel::ClickableLabel(QWidget *parent, Qt::WindowFlags f)
    : QLabel(parent)
{

}

void ClickableLabel::mousePressEvent(QMouseEvent* event) {
    active = true;
    emit touch(event->pos());
}
void ClickableLabel::mouseMoveEvent(QMouseEvent* event) {
    if(active) {
        emit touch(event->pos());
    }
}
void ClickableLabel::mouseReleaseEvent(QMouseEvent* event) {
    active = false;
    emit release();
}

void ClickableLabel::wheelEvent(QWheelEvent *event) {
    event->accept();
    emit wheely(event->angleDelta().y());
}
