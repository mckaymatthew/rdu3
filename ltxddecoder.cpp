#include "ltxddecoder.h"
#include <QtEndian>

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
    const bool isTouch = (payload.length() == 6) && (payload[0] == 0x13);
    if(isTouch) {
        touchPoint();
    } else {
        emit logMessage(QString("Got unkown LTXD Bytes: %1 %2").arg(payload.size()).arg(payload.toHex()));
    }
}
void LtxdDecoder::touchPoint() {
    emit logMessage(QString("Touch Payload: %1 fe%2fd").arg(payload.size()).arg(payload.toHex()));
    uint16_t* payloadWord = (uint16_t*) payload.data();
    uint16_t x = qFromBigEndian<uint16_t>(payloadWord[1]);
    uint16_t y = qFromBigEndian<uint16_t>(payloadWord[2]);
    const bool isTouchRelease = (x == 9999) && (y == 9999);
    if(isTouchRelease) {
        emit logMessage(QString("Touch: Release"));
    } else {
        //13 00 0186 00f1
        emit logMessage(QString("Touch: X %1, Y %2").arg(x).arg(272-y));
    }
}
