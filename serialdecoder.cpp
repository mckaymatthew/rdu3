#include "serialdecoder.h"

SerialDecoder::SerialDecoder(QObject *parent) : QObject(parent)
{

}

void SerialDecoder::newData(QByteArray ltxd) {
    for(auto b: ltxd) {
        processByte(b);
    }
}

void SerialDecoder::processByte(unsigned char byte) {
    if(searching && byte == 0xFE) {
        searching = false;
    } else if(!searching && byte == 0xFD) {
        //Got whole thing.
        handlePayload(payload);
        searching = true;
        payload.clear();
    } else if (!searching) {
        payload.append(byte);
    }
}
