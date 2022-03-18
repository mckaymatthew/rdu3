#ifndef SERIALDECODER_H
#define SERIALDECODER_H

#include <QObject>

class SerialDecoder : public QObject
{
    Q_OBJECT
public:
    explicit SerialDecoder(QObject *parent = nullptr);

public slots:
    void newData(QByteArray);
protected:
    virtual void handlePayload(QByteArray p) = 0;
private:
    bool searching = true;
    void processByte(unsigned char byte);
    QByteArray payload;

};

#endif // SERIALDECODER_H
