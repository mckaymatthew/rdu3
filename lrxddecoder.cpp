#include "lrxddecoder.h"
#include <QtEndian>
#include <QDebug>

LrxdDecoder::LrxdDecoder(QObject *parent)
    : SerialDecoder{parent}
{

}


void LrxdDecoder::handlePayload(QByteArray payload) {
    constexpr uint8_t pwerLedPayload = 0x01;
    const uint8_t payloadId = payload[0];
    const bool isPwrLed = (payload.length() == 2) && (payloadId == pwerLedPayload);
    if(isPwrLed) {
        handlePwrLed(payload);
    } else {
        QString payloadReadable = payload.toHex();
        qInfo() << (QString("Got unkown LRXD Bytes: %1 %2").arg(payload.size()).arg(payloadReadable));
    }
}

void LrxdDecoder::handlePwrLed(QByteArray payload) {
//    const bool isOff = payload[1] == 0x02;
    const bool isOn = payload[1] == 0x03;
//    qInfo() << QString("LRXD: Power button light: %1").arg(isOn?"On":"Off");
}
