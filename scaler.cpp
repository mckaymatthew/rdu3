#include "scaler.h"
#include <QDebug>
#include "RDUConstants.h"

scaler::scaler(RDUWorker* worker, QObject *parent) : QObject(parent),
    m_worker(worker)
{

}

void scaler::startWorker()
{
}
void scaler::resized(QSize s) {
     m_s = s;
}
void scaler::newWork() {
    auto workToDo = m_worker->getCopy(m_framebuffer);
    if(workToDo) {

        QImage img((const uchar*) m_framebuffer.data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
        m_resizeTime.restart();
        m_frameBufferImage = img.scaled(m_s.width(),m_s.height(),Qt::KeepAspectRatio);
        m_resizeTimeLast =
                ((59.0* m_resizeTimeLast) +
                (m_resizeTime.nsecsElapsed() / 1000000)) / 60;
        g_ResizeTime = m_resizeTimeLast * g_scaleFactor;

        if(m_writeBuffer) {
            m_bufferOne = m_frameBufferImage.copy();
        } else {
            m_bufferTwo = m_frameBufferImage.copy();
        }
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
