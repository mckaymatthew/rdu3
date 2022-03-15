#ifndef RDUWORKER_H
#define RDUWORKER_H

#include <QObject>
#include <QUdpSocket>
#include <QFile>
#include <QElapsedTimer>
#include <QMutex>

class RDUWorker : public QObject
{
    Q_OBJECT
public:
    explicit RDUWorker(QObject *parent = nullptr);
    bool getCopy(QByteArray& r);
public slots:
    void startWorker();
    void logPacketData(bool state);
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

    QFile* m_logFile;
    QTextStream* m_stream;
    bool m_logCsv;
    bool m_fresh;

    QElapsedTimer m_framesStart;

    bool m_writeBuffer;

    QMutex m_copyMux;

    uint16_t m_packetIdLast;
};

#endif // RDUWORKER_H
