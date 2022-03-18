#include "rduworker.h"
#include "RDUConstants.h"
#include <QNetworkDatagram>

namespace {
constexpr int globalsUpdatePerSecond = 10;
constexpr int globalsUpdateMs = 1000/10;
}
RDUWorker::RDUWorker(QObject *parent)
    : QObject(parent)
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
        qInfo() << QString("Failed to open port\nApp already open?");
    } else {
        qInfo() << QString("Started Network Worker Thread.");
    }
    m_fpsCounter.start();
    startTimer(globalsUpdateMs);
    pkt.resize(BYTES_PER_LINE + BYTES_PACKET_OVERHEAD);

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
void RDUWorker::csvLog(uint16_t line, uint16_t packet) {
    if(m_logCsv) {
        if(!m_stream) {
            m_logFile = new QFile("log.csv",this);
            auto openRet = m_logFile->open(QIODevice::ReadWrite | QIODevice::Truncate);
            if(openRet) {
                qInfo() << QString("Debug: High speed network metadata logging.");
                m_stream = new QTextStream(m_logFile);
                *m_stream << "Packet,InternalCtr,Time,LineNumber\n";
            }
        }
        if(m_stream) {
            auto time = m_framesStart.nsecsElapsed();
            *m_stream << QString("%1,%4,%2,%3\n").arg(g_packetsTotal).arg(time).arg(line).arg(packet);
        }
    }
}
void RDUWorker::processPendingDatagrams()
{
    while (true) { //hasPendingDatagrams() is expensive on Windows? Just use return value of read
        auto bytesRead = m_incoming->readDatagram(pkt.data(), pkt.size(),nullptr,nullptr);
        if (bytesRead == -1) {
            return;
        }
        g_packetsTotal++;
        const bool packetWrongSize = bytesRead != BYTES_PER_LINE + BYTES_PACKET_OVERHEAD;
        if(packetWrongSize) {
            g_packetsRejected++;
            continue;
        }
        m_statsBytes = m_statsBytes + bytesRead + 28; //fixed ip + udp header
        auto header = pkt.data() + 2 ;
        const uint16_t packetCtr = *((uint16_t*)header);
        const uint16_t lineField = *((uint16_t*)header+1);
        const uint32_t scanlineIdx = BYTES_PER_LINE * lineField;

        const bool oooPacket = (packetCtr < m_packetIdLast) && ((packetCtr != 0) && (m_packetIdLast != 65535));
        const bool badLine = lineField > LINES;
        const bool endOfFrame = lineField == LINES-1;
        const bool currentBuffValid = m_currentBuff != nullptr;
        csvLog(lineField,packetCtr);
        if(oooPacket) {
            qInfo() << QString("OOO packet %1, %2, %3").arg(packetCtr).arg(m_packetIdLast).arg(lineField);
        }
        m_packetIdLast = packetCtr;

        if(badLine) {
            g_packetsRejected++;
            qInfo() << QString("Bad packet %1").arg(lineField);
            continue;
        }
        m_statsLines++;
        auto pixelData = pkt.right(BYTES_PER_LINE);
        if(currentBuffValid) {
            m_currentBuff->replace(scanlineIdx,BYTES_PER_LINE,pixelData);
        }
        if(endOfFrame) {
            if(currentBuffValid) {
                emit newFrame(m_currentBuff);
            } else {
                g_FramesLostNoBuffer++;
            }
            m_currentBuff = !m_buffsAvail.isEmpty() ? m_buffsAvail.dequeue() : nullptr;
            g_NetworkFramesPerSecond =
                    (((9* g_NetworkFramesPerSecond) +
                    ((1000000000.0/m_fpsCounter.nsecsElapsed()) * g_scaleFactor)))
                    /globalsUpdatePerSecond;
            m_fpsCounter.restart();
            g_NetworkFramesTotal++;
        }
    }
}
