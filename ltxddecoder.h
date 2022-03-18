#ifndef LTXDDECODER_H
#define LTXDDECODER_H

#include <QObject>
#include <QByteArray>
#include "serialdecoder.h"

class LtxdDecoder : public SerialDecoder
{
    Q_OBJECT
public:
    explicit LtxdDecoder(QObject *parent = nullptr);
protected:
    void handlePayload(QByteArray) override;
private:
    void touchPoint(QByteArray payload);
};

#endif // LTXDDECODER_H
