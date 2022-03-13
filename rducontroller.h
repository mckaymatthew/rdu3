#ifndef RDUCONTROLLER_H
#define RDUCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QStateMachine>
#include <QHostInfo>
#include <QByteArray>
#include <QTcpSocket>
#include "simple.pb.h"
#include "LtxdDecoder.h"
#include <qmdnsengine/server.h>
#include <qmdnsengine/resolver.h>


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
    void startController();
    void spinMainDial(int ticks);
    void setFrameDivisor(uint8_t ndivisor);

    void notifyTimeout();
    void notifySocketError();
    void notifyPingTimeout();
    void notifyFPSTimeout();
    void notifyMdnsTimeout();
    void notifyConnectTimeout();
    void notifyConnected();

    void doQueryMdns();
    void doConnectToRdu();
    void doPing();
    void doClkInhibit();
    void doSetup();
    void doClkEnable();
    void doSetFps();

signals:
    void RDUReady();
    void RDUNotReady();

    void foundHost();
    void pingResponse();
    void gotAck();
    void notifyUserOfState(QString);
private:
    QList<QSharedPointer<QState>> m_states;
    QTimer mdnsQueryTimeout;
    QHostInfo rduHost;
    QTcpSocket socket;
    QTimer periodicPing;
    QTimer pingTimeout;
    QTimer setupTimeout;
    QTimer connectTimeout;
    QTimer fpsTimeout;

    QByteArray msg_resp_buffer;
    int msg_resp_buffer_write = -1;
    int msg_resp_buffer_idx = 0;
    uint8_t divisor = 0;
    LtxdDecoder m_ltxd_decoder;
    int m_dial_offset = 0;

    //Ensure this is the first item destroyed.
    QStateMachine machine;
    void setupStateMachine();

    QMdnsEngine::Server mServer;
    QMdnsEngine::Resolver *mResolver = nullptr;
    QHostAddress m_addrAlt;
};

#endif // RDUCONTROLLER_H
