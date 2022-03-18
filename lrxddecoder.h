#ifndef LrxdDecoder_H
#define LrxdDecoder_H

#include <QObject>
#include <QByteArray>
#include "serialdecoder.h"

class LrxdDecoder : public SerialDecoder
{
    Q_OBJECT
public:
    explicit LrxdDecoder(QObject *parent = nullptr);
protected:
    void handlePayload(QByteArray) override;
private:
    void handlePwrLed(QByteArray payload);
};

#endif // LrxdDecoder_H
