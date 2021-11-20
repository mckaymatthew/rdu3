#ifndef RDUCONTROLLER_H
#define RDUCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QStateMachine>
#include <QHostInfo>
#include <QByteArray>
#include <QTcpSocket>
#include "simple.pb.h"

class RDUController : public QObject
{
    Q_OBJECT
public:
    explicit RDUController(QObject *parent = nullptr);
public slots:
    void readyRead();

    void writeWord(uint32_t addr, uint32_t data);
    void writeRequest(Request r);
    void writeInject(QByteArray toInject);
    void writeInjectHex(QString toInjectHex);

signals:
    void foundHost();
    void pingResponse();
    void gotAck();
    void logMessage(QString);
private:
    QStateMachine machine;
    QTimer mdnsQueryTimeout;
    QHostInfo rduHost;
    QTcpSocket socket;
    QTimer periodicPing;
    QTimer pingTimeout;
    QTimer setupTimeout;

    QByteArray msg_resp_buffer;
    int msg_resp_buffer_write = -1;
    int msg_resp_buffer_idx = 0;

    void setupStateMachine();

    uint32_t CLK_GATE = 0xf0003000;
    uint32_t CPU_RESET = 0xf0000800;
};

#endif // RDUCONTROLLER_H
