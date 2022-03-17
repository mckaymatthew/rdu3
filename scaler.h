#ifndef SCALER_H
#define SCALER_H

#include <QObject>
#include <QMutex>
#include <QImage>
#include <QRect>
#include "rduworker.h"

class scaler : public QObject
{
    Q_OBJECT
public:
    explicit scaler(RDUWorker* worker, QObject *parent = nullptr);

    bool getCopy(QImage& r);
public slots:
    void startWorker();
    void newWork();
    void resized(QSize s);
signals:
    void newFrame();
private:
    void nonallocResize(QImage* in, QImage& out);
    uint32_t m_notPickedUp = 0;
    uint32_t m_nothingToPickup = 0;
    RDUWorker* m_worker;
    QSize m_s;
    bool m_fresh = false;
    bool m_writeBuffer = false;
    QMutex m_copyMux;
    QByteArray m_framebuffer;
    QImage* m_frameBufferImage = nullptr;
    QImage m_bufferOne;
    QImage m_bufferTwo;
    QElapsedTimer m_resizeTime;
    double m_resizeTimeLast = 0;

};

#endif // SCALER_H
