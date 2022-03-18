#ifndef RDUWORKER_H
#define RDUWORKER_H

#include <QObject>
#include <QUdpSocket>
#include <QFile>
#include <QElapsedTimer>
#include <QMutex>
#include <QTimerEvent>
#include <QQueue>

class RDUWorker : public QObject
{
    Q_OBJECT
public:
    explicit RDUWorker(QObject *parent = nullptr);
public slots:
    void startWorker();
    void logPacketData(bool state);
    void buffDispose(QByteArray* d);
protected:
    void timerEvent(QTimerEvent *event) override;
signals:
    void newFrame(QByteArray* frame);
    void newStats(uint32_t packetCount, uint32_t badPackets, uint32_t oooPackets);

private slots:
    void processPendingDatagrams();
    void csvLog(uint16_t line, uint16_t packet);
private:
    QUdpSocket* m_incoming = nullptr;

    bool m_logCsv = false;
    QFile* m_logFile = nullptr;
    QTextStream* m_stream = nullptr;

    QElapsedTimer m_framesStart;

    uint16_t m_packetIdLast;

    QByteArray pkt;

    double m_statsBytes = 0;
    double m_statsLines = 0;

    double bytesPerSecond_p = 0;
    double linesPerSecond_p = 0;
    QElapsedTimer m_fpsCounter;

    QQueue<QByteArray* > m_buffsAvail = {};
    QByteArray* m_currentBuff = nullptr;
};

#endif // RDUWORKER_H
