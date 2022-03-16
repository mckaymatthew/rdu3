#ifndef RDUWORKER_H
#define RDUWORKER_H

#include <QObject>
#include <QUdpSocket>
#include <QFile>
#include <QElapsedTimer>
#include <QMutex>
#include <QTimerEvent>

class RDUWorker : public QObject
{
    Q_OBJECT
public:
    explicit RDUWorker(QObject *parent = nullptr);
    bool getCopy(QByteArray& r);
public slots:
    void startWorker();
    void logPacketData(bool state);
protected:
    void timerEvent(QTimerEvent *event) override;
signals:
    void newFrame();
    void newStats(uint32_t packetCount, uint32_t badPackets, uint32_t oooPackets);
    void message(QString);

private slots:
    void processPendingDatagrams();
private:
    QUdpSocket* m_incoming;
    QByteArray m_bufferOne;
    QByteArray m_bufferTwo;
    uint32_t m_packetCount;
    uint32_t m_badPackets;
    uint32_t m_oooPackets;
    uint32_t m_missingPackets;

    uint32_t m_frameCount = 0;
    uint32_t m_notPickedUp = 0;
    uint32_t m_nothingToPickup = 0;

    QFile* m_logFile;
    QTextStream* m_stream;
    bool m_logCsv;
    bool m_fresh;

    QElapsedTimer m_framesStart;

    bool m_writeBuffer;

    QMutex m_copyMux;

    uint16_t m_packetIdLast;

    QByteArray pkt;

    double m_statsBytes = 0;
    double m_statsLines = 0;

    double bytesPerSecond_p = 0;
    double linesPerSecond_p = 0;
    QElapsedTimer m_fpsCounter;
};

#endif // RDUWORKER_H
