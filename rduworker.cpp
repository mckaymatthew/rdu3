#include "rduworker.h"
#include <QNetworkDatagram>

namespace {
    constexpr uint32_t LINES = 288;
    constexpr uint32_t COLUMNS = 480;
    constexpr uint32_t BYTES_PER_PIXEL = 2; //RGB565
    constexpr uint32_t BYTES_PER_LINE = COLUMNS * BYTES_PER_PIXEL;
    constexpr uint32_t BYTES_PER_FRAME = BYTES_PER_LINE * LINES;
    constexpr uint32_t PACKET_OVERHEAD = 1;

}

RDUWorker::RDUWorker(QObject *parent)
    : QObject(parent)
    , m_incoming()
    , m_bufferOne(BYTES_PER_FRAME,'\0')
    , m_bufferTwo(BYTES_PER_FRAME,'\0')
    , m_packetCount(0)
    , m_badPackets(0)
    , m_logFile(nullptr)
    , m_stream(nullptr)
    , m_logCsv(false)
    , m_framesStart()
    , m_writeBuffer(false)
{

}
void RDUWorker::startWorker()
{
    bool result = m_incoming.bind(QHostAddress::AnyIPv4, 1337);
    if(!result) {
        emit message(QString("Failed to open port\nApp already open?"));
    }
    connect(&m_incoming, &QUdpSocket::readyRead, this, &RDUWorker::processPendingDatagrams);

}

void RDUWorker::logPacketData(bool state)
{
    m_logCsv = state;
}

void RDUWorker::processPendingDatagrams()
{
    while (m_incoming.hasPendingDatagrams()) {
        m_packetCount++;
        QNetworkDatagram datagram = m_incoming.receiveDatagram();
        if(datagram.data().size() != (BYTES_PER_FRAME + PACKET_OVERHEAD)) {
            m_badPackets++;
            continue;
        }
        auto pkt = datagram.data();
        auto header = pkt.data() + 2 ;
        const uint32_t lineField = *((uint32_t*)header);
//        const uint16_t* asRGB = (const uint16_t*)(pkt.data() + 6);

        if(lineField > LINES) {
            m_badPackets++;
            continue;
        }

        if(m_logCsv) {
            if(!m_stream) {

                m_logFile = new QFile("log.csv",this);
                auto openRet = m_logFile->open(QIODevice::ReadWrite | QIODevice::Truncate);
                if(openRet) {
                    emit message(QString("Open Log CSV."));
                    m_stream = new QTextStream(m_logFile);
                    *m_stream << "Packet,Time,LineNumber\n";
                }
            }
            if(m_stream) {
                auto time = m_framesStart.nsecsElapsed();
                *m_stream << QString("%1,%2,%3\n").arg(m_packetCount).arg(time).arg(lineField);
            }
        }

        uint32_t scanlineIdx = BYTES_PER_LINE * lineField;
        auto pixelData = pkt.right(BYTES_PER_LINE);
        if(m_writeBuffer) {
            m_bufferOne.replace(scanlineIdx,pixelData);
        } else {
            m_bufferTwo.replace(scanlineIdx,pixelData);
        }
    }
}
