#ifndef RDUCONTROLLER_H
#define RDUCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QStateMachine>
#include <QHostInfo>
#include <QByteArray>
#include <QTcpSocket>
#include "simple.pb.h"
#include "qMDNS.h"


class RDUController : public QObject
{
    Q_OBJECT
public:
    explicit RDUController(QObject *parent = nullptr);
public slots:
    void readyRead();

//    void connect();
//    void disconnect();

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
    QList<QSharedPointer<QState>> m_states;
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
    qMDNS* m_mDNS;

    void setupStateMachine();
};

#endif // RDUCONTROLLER_H
