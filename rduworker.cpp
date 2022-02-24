#include "rduworker.h"
#include "RDUConstants.h"
#include <QNetworkDatagram>


RDUWorker::RDUWorker(QObject *parent)
    : QObject(parent)
    , m_incoming(nullptr)
    , m_bufferOne(BYTES_PER_FRAME,'\0')
    , m_bufferTwo(BYTES_PER_FRAME,'\0')
    , m_packetCount(0)
    , m_oooPackets(0)
    , m_badPackets(0)
    , m_logFile(nullptr)
    , m_stream(nullptr)
    , m_logCsv(false)
    , m_framesStart()
    , m_writeBuffer(false)
    , m_packetIdLast(0)
{

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
}

void RDUWorker::logPacketData(bool state)
{
    m_logCsv = state;
}

void RDUWorker::processPendingDatagrams()
{
    QByteArray pkt;
    pkt.resize(BYTES_PER_LINE + BYTES_PACKET_OVERHEAD);
    while (m_incoming->hasPendingDatagrams()) {
//        pkt.resize(m_incoming->pendingDatagramSize());
        auto bytesRead = m_incoming->readDatagram(pkt.data(), pkt.size(),nullptr,nullptr);
        m_packetCount++;
//        QNetworkDatagram datagram = m_incoming->receiveDatagram();
        if(bytesRead != BYTES_PER_LINE + BYTES_PACKET_OVERHEAD) {
            m_badPackets++;
            continue;
        }
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
            continue;
        }

        if(lineField > LINES) {
            m_badPackets++;
            continue;
        }

        uint32_t scanlineIdx = BYTES_PER_LINE * lineField;
        auto pixelData = pkt.right(BYTES_PER_LINE);
        if(m_writeBuffer) {
            m_bufferOne.replace(scanlineIdx,BYTES_PER_LINE,pixelData);
        } else {
            m_bufferTwo.replace(scanlineIdx,BYTES_PER_LINE,pixelData);
        }
        if(lineField == LINES-1) {
            {
                QMutexLocker locker(&m_copyMux);
                m_writeBuffer = !m_writeBuffer;
            }
            emit newFrame();
        }
    }
    emit newStats(m_packetCount,m_badPackets, m_oooPackets);
}

QByteArray RDUWorker::getCopy()
{
    QMutexLocker locker(&m_copyMux);
    char * rawPtr = nullptr;
    if(m_writeBuffer) {
        rawPtr = m_bufferTwo.data();
    } else {
        rawPtr = m_bufferOne.data();
    }
    QByteArray cpy(rawPtr, BYTES_PER_FRAME);
    return cpy;
}
