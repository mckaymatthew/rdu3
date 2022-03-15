#include "ltxddecoder.h"
#include <QtEndian>
#include <QDebug>

LtxdDecoder::LtxdDecoder(QObject *parent)
    : QObject{parent}
{

}

void LtxdDecoder::newData(QByteArray ltxd) {
    for(auto b: ltxd) {
        processByte(b);
    }
}

void LtxdDecoder::processByte(unsigned char byte) {
    if(searching && byte == 0xFE) {
        searching = false;
    } else if(!searching && byte == 0xFD) {
        //Got whole thing.
        handlePayload();
        searching = true;
        payload.clear();
    } else if (!searching) {
        payload.append(byte);
    }
}

void LtxdDecoder::handlePayload() {
    constexpr uint8_t touchPayload = 0x13;
    const uint8_t payloadId = payload[0];
    const bool isTouch = (payload.length() == 6) && (payloadId == touchPayload);
    if(isTouch) {
        touchPoint();
    } else {
        QString payloadReadable = payload.toHex();
        qInfo() << (QString("Got unkown LTXD Bytes: %1 %2").arg(payload.size()).arg(payloadReadable));
    }
}
void LtxdDecoder::touchPoint() {
    QString payloadReadable = payload.toHex();
    qInfo() << (QString("Touch Payload: %1 fe%2fd").arg(payload.size()).arg(payloadReadable));
    uint16_t* payloadWord = (uint16_t*) payload.data();
    uint16_t x = qFromBigEndian<uint16_t>(payloadWord[1]);
    uint16_t y = qFromBigEndian<uint16_t>(payloadWord[2]);
    const bool isTouchRelease = (x == 9999) && (y == 9999);
    if(isTouchRelease) {
        qInfo() << (QString("Touch: Release"));
    } else {
        //13 00 0186 00f1
        qInfo() << (QString("Touch: X %1, Y %2").arg(x).arg(272-y));
    }
}
