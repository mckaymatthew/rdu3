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


    void writeWord(uint32_t addr, uint32_t data);
    void writeRequest(Request r);
    void writeInject(QByteArray toInject);
    void writeInjectHex(QString toInjectHex);

    void notifyTimeout();
    void notifySocketError();
    void notifyPingTimeout();
    void doQueryMdns();
    void notifyMdnsTimeout();
    void doConnectToRdu();
    void notifyConnectTimeout();
    void notifyConnected();
    void doPing();
    void doClkInhibit();
    void doSetup();
    void doClkEnable();

signals:
    void RDUReady();
    void RDUNotReady();

    void foundHost();
    void pingResponse();
    void gotAck();
    void logMessage(QString);
    void logMessageWithError(QString, QString);
private:
    QList<QSharedPointer<QState>> m_states;
    QStateMachine machine;
    QTimer mdnsQueryTimeout;
    QHostInfo rduHost;
    QTcpSocket socket;
    QTimer periodicPing;
    QTimer pingTimeout;
    QTimer setupTimeout;
    QTimer connectTimeout;

    QByteArray msg_resp_buffer;
    int msg_resp_buffer_write = -1;
    int msg_resp_buffer_idx = 0;
    qMDNS* m_mDNS;

    void setupStateMachine();
};

#endif // RDUCONTROLLER_H
