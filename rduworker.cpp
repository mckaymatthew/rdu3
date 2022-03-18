#include "rduworker.h"
#include "RDUConstants.h"
#include <QNetworkDatagram>

namespace {
constexpr int globalsUpdatePerSecond = 10;
constexpr int globalsUpdateMs = 1000/10;
}
RDUWorker::RDUWorker(QObject *parent)
    : QObject(parent)
    , m_incoming(nullptr)
    , m_packetCount(0)
    , m_oooPackets(0)
    , m_badPackets(0)
    , m_logFile(nullptr)
    , m_stream(nullptr)
    , m_logCsv(false)
    , m_framesStart()
    , m_packetIdLast(0)
{
    for(int i = 0; i < 10; i++) {
        m_buffsAvail.enqueue(new QByteArray(BYTES_PER_FRAME,'\0'));
    }
    m_currentBuff = m_buffsAvail.dequeue();
}

void RDUWorker::startWorker()
{
    m_incoming = new QUdpSocket();
    connect(m_incoming, &QUdpSocket::readyRead, this, &RDUWorker::processPendingDatagrams);
    m_framesStart.start();
    bool result = m_incoming->bind(QHostAddress::AnyIPv4, 1337);
    if(!result) {
        emit message(QString("Failed to open port\nApp already open?"));
    } else {
        emit message(QString("Started Network Worker Thread."));
    }
    m_fpsCounter.start();
    startTimer(globalsUpdateMs);

}

void RDUWorker::timerEvent(QTimerEvent *event) {
    double bytesPerSecond = m_statsBytes * globalsUpdatePerSecond;
    double linesPerSecond = m_statsLines * globalsUpdatePerSecond;
//    qInfo() << QString("Instant BPS: %1, LPS: %2, Frames: %4").arg(bytesPerSecond).arg(linesPerSecond).arg(g_NetworkFramesTotal);
    auto oldWeight = globalsUpdatePerSecond-1;
    auto newWeight = 1;
    bytesPerSecond =  (((oldWeight* bytesPerSecond_p) + (newWeight* bytesPerSecond)))/globalsUpdatePerSecond;
    linesPerSecond =  (((oldWeight* linesPerSecond_p) + (newWeight* linesPerSecond)))/globalsUpdatePerSecond;
//    qInfo() << QString("Averaged BPS: %1, LPS: %2, Frames: %3")
//               .arg(bytesPerSecond).arg(linesPerSecond).arg(g_NetworkFramesTotal);

    g_NetworkBytesPerSecond = bytesPerSecond * g_scaleFactor;
    g_NetworkLinesPerSecond = linesPerSecond * g_scaleFactor;

    bytesPerSecond_p = bytesPerSecond;
    linesPerSecond_p = linesPerSecond;
    m_statsBytes = 0;
    m_statsLines = 0;
};
void RDUWorker::logPacketData(bool state)
{
    m_logCsv = state;
}

void RDUWorker::buffDispose(QByteArray* d) {
    if(d != nullptr) {
        m_buffsAvail.enqueue(d);
    }
}
void RDUWorker::processPendingDatagrams()
{
    pkt.resize(BYTES_PER_LINE + BYTES_PACKET_OVERHEAD);
    while (true) { //hasPendingDatagrams() is expensive on Windows? Just use return value of read
        auto bytesRead = m_incoming->readDatagram(pkt.data(), pkt.size(),nullptr,nullptr);
        if (bytesRead == -1) {
            return;
        }
        if(bytesRead != BYTES_PER_LINE + BYTES_PACKET_OVERHEAD) {
            continue;
        }
        m_statsBytes = m_statsBytes + bytesRead + 28; //fixed ip + udp header
        m_packetCount++;
        auto header = pkt.data() + 2 ;
        const uint16_t packetCtr = *((uint16_t*)header);
        const uint16_t lineField = *((uint16_t*)header+1);

        if(m_logCsv) {
            if(!m_stream) {
                m_logFile = new QFile("log.csv",this);
                auto openRet = m_logFile->open(QIODevice::ReadWrite | QIODevice::Truncate);
                if(openRet) {
                    emit message(QString("Open Log CSV."));
                    m_stream = new QTextStream(m_logFile);
                    *m_stream << "Packet,InternalCtr,Time,LineNumber\n";
                }
            }
            if(m_stream) {
                auto time = m_framesStart.nsecsElapsed();
                *m_stream << QString("%1,%4,%2,%3\n").arg(m_packetCount).arg(time).arg(lineField).arg(packetCtr);
            }
        }

        const bool oooPacket = packetCtr < m_packetIdLast;
        m_packetIdLast = packetCtr;
        if(oooPacket) {
            m_oooPackets++;
            qInfo() << QString("OOO packet %1, %2").arg(packetCtr).arg(m_packetIdLast);
//            continue;
        }

        if(lineField > LINES) {
            m_badPackets++;
            qInfo() << QString("Bad packet %1").arg(lineField);
            continue;
        }

        m_statsLines++;
        uint32_t scanlineIdx = BYTES_PER_LINE * lineField;
        auto pixelData = pkt.right(BYTES_PER_LINE);
        if(m_currentBuff != nullptr) {
            m_currentBuff->replace(scanlineIdx,BYTES_PER_LINE,pixelData);
        }
        if(lineField == LINES-1) {
            if(m_currentBuff != nullptr) {
                emit newFrame(m_currentBuff);
            } else {
                qInfo() << QString("Frame lost to eternity");
            }
            if(!m_buffsAvail.isEmpty()) {
                m_currentBuff = m_buffsAvail.dequeue();
            } else {
                m_currentBuff = nullptr;
            }
            m_frameCount++;
            g_NetworkFramesPerSecond =
                    (((9* g_NetworkFramesPerSecond) +
                    ((1000000000.0/m_fpsCounter.nsecsElapsed()) * g_scaleFactor)))
                    /globalsUpdatePerSecond;
            m_fpsCounter.restart();
            g_NetworkFramesTotal++;
        }
    }
}
