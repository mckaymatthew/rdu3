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
    pkt.resize(BYTES_PER_LINE + BYTES_PACKET_OVERHEAD);
    while (true) { //hasPendingDatagrams() is expensive on Windows? Just use return value of read
        auto bytesRead = m_incoming->readDatagram(pkt.data(), pkt.size(),nullptr,nullptr);
        if (bytesRead == -1) {
            return;
        }
        if(bytesRead != BYTES_PER_LINE + BYTES_PACKET_OVERHEAD) {
            continue;
        }
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
                m_fresh = true;
            }
            emit newFrame();
        }
    }
}

bool RDUWorker::getCopy(QByteArray& r)
{
    QMutexLocker locker(&m_copyMux);
    const bool newData = m_fresh;
    m_fresh = false;
    if(newData) {
        if(m_writeBuffer) {
            r.replace(0,BYTES_PER_FRAME,m_bufferTwo);
        } else {
            r.replace(0,BYTES_PER_FRAME,m_bufferOne);
        }
    } else {
        qInfo() << "Dropped frame?";
    }
    return newData;


}
