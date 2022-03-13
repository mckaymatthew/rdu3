#ifndef LTXDDECODER_H
#define LTXDDECODER_H

#include <QObject>
#include <QByteArray>

class LtxdDecoder : public QObject
{
    Q_OBJECT
public:
    explicit LtxdDecoder(QObject *parent = nullptr);
public slots:
    void newData(QByteArray);
private:
    bool searching = true;
    void processByte(unsigned char byte);
    void handlePayload();
    void touchPoint();
    QByteArray payload;
};

#endif // LTXDDECODER_H
