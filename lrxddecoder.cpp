#include "lrxddecoder.h"
#include <QtEndian>
#include <QDebug>

LrxdDecoder::LrxdDecoder(QObject *parent)
    : QObject{parent}
{

}

void LrxdDecoder::newData(QByteArray ltxd) {
    for(auto b: ltxd) {
        processByte(b);
    }
}

void LrxdDecoder::processByte(unsigned char byte) {
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

void LrxdDecoder::handlePayload() {
    constexpr uint8_t pwerLedPayload = 0x01;
    const uint8_t payloadId = payload[0];
    const bool isPwrLed = (payload.length() == 2) && (payloadId == pwerLedPayload);
    if(isPwrLed) {
        handlePwrLed();
    } else {
        QString payloadReadable = payload.toHex();
        qInfo() << (QString("Got unkown LRXD Bytes: %1 %2").arg(payload.size()).arg(payloadReadable));
    }
}

void LrxdDecoder::handlePwrLed() {
//    const bool isOff = payload[1] == 0x02;
    const bool isOn = payload[1] == 0x03;
    qInfo() << QString("LRXD: Power button light: %1").arg(isOn?"On":"Off");
}
