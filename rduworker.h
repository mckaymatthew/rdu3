#ifndef RDUWORKER_H
#define RDUWORKER_H

#include <QObject>
#include <QUdpSocket>
#include <QFile>
#include <QElapsedTimer>

class RDUWorker : public QObject
{
    Q_OBJECT
public:
    explicit RDUWorker(QObject *parent = nullptr);
public slots:
    void startWorker();
    void logPacketData(bool state);
signals:
    void newFrame();
    void newStats(uint32_t packetCount, uint32_t badPackets);
    void message(QString);

private slots:
    void processPendingDatagrams();
private:
    QUdpSocket m_incoming;
    QByteArray m_bufferOne;
    QByteArray m_bufferTwo;
    uint32_t m_packetCount;
    uint32_t m_badPackets;

    QFile* m_logFile;
    QTextStream* m_stream;
    bool m_logCsv;

    QElapsedTimer m_framesStart;

    bool m_writeBuffer;

};

#endif // RDUWORKER_H
