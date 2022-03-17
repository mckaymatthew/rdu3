#include "scaler.h"
#include <QDebug>
#include "RDUConstants.h"
#include <QPainter>

scaler::scaler(RDUWorker* worker, QObject *parent) : QObject(parent),
    m_worker(worker)
{

}

void scaler::startWorker()
{
}
void scaler::resized(QSize s) {

    QSize newSize(COLUMNS, LINES);
    newSize.scale(s,Qt::KeepAspectRatio);
    newSize.rwidth() = qMax(newSize.width(), 1);
    newSize.rheight() = qMax(newSize.height(), 1);
    m_s = newSize;
}

void scaler::nonallocResize(QImage* in, QImage& out) {

    QTransform wm = QTransform::fromScale((qreal)m_s.width() / COLUMNS, (qreal)m_s.height() / LINES);
    QTransform mat = in->trueMatrix(wm, COLUMNS, LINES);
    auto hd = qRound(qAbs(mat.m22()) * m_s.height());
    auto wd = qRound(qAbs(mat.m11()) * m_s.width());
    auto outSize = out.size();
    if(wd != outSize.width() || hd != outSize.height()) {
        qInfo() << "Out image wrong size, reconstructing";
        out = QImage(wd, hd, in->format());
    }

    QPainter p(&out);
    p.setTransform(mat);
    p.drawImage(QPoint(0, 0), *in);
    return;

}
void scaler::newWork() {
    auto workToDo = m_worker->getCopy(m_framebuffer);
    if(workToDo) {
        m_resizeTime.restart();
        if(m_frameBufferImage == nullptr) {
            m_frameBufferImage = new QImage((const uchar*) m_framebuffer.data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
        }
        //            QImage img((const uchar*) m_framebuffer.data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
        if(m_writeBuffer) {
            nonallocResize(m_frameBufferImage,m_bufferOne);
//            m_bufferOne = m_frameBufferImage->scaled(m_s.width(),m_s.height(),Qt::KeepAspectRatio);
        } else {
            nonallocResize(m_frameBufferImage,m_bufferTwo);
//            m_bufferTwo = m_frameBufferImage->scaled(m_s.width(),m_s.height(),Qt::KeepAspectRatio);
        }
        m_resizeTimeLast =
                ((59.0* m_resizeTimeLast) +
                (m_resizeTime.nsecsElapsed() / 1000000)) / 60;
        g_ResizeTime = m_resizeTimeLast * g_scaleFactor;

        {
            QMutexLocker locker(&m_copyMux);
            m_writeBuffer = !m_writeBuffer;
            if(m_fresh) {
                m_notPickedUp++;
                qInfo() << QString("Scaler Frame not picked up! %1").arg(m_notPickedUp);
            }
            m_fresh = true;
        }
        emit newFrame();
    }
}
bool scaler::getCopy(QImage& r)
{
    QMutexLocker locker(&m_copyMux);
    const bool newData = m_fresh;
    m_fresh = false;
    if(newData) {
        if(m_writeBuffer) {
            r = m_bufferTwo.copy();
        } else {
            r = m_bufferOne.copy();
        }
    } else {
        qInfo() << QString("Scaler: Nothing to pick up? %1");
    }
    return newData;


}
