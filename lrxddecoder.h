#ifndef LrxdDecoder_H
#define LrxdDecoder_H

#include <QObject>
#include <QByteArray>

class LrxdDecoder : public QObject
{
    Q_OBJECT
public:
    explicit LrxdDecoder(QObject *parent = nullptr);
public slots:
    void newData(QByteArray);
private:
    bool searching = true;
    void processByte(unsigned char byte);
    void handlePayload();
    QByteArray payload;
    void handlePwrLed();
};

#endif // LrxdDecoder_H
