#ifndef RDUCONTROLLER_H
#define RDUCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QHostInfo>
#include <QByteArray>
#include <QTcpSocket>
#include "simple.pb.h"
#include <qmdnsengine/server.h>
#include <qmdnsengine/resolver.h>
#include <QElapsedTimer>
#include <QPoint>


class RDUController : public QObject
{
    Q_OBJECT
public:
    explicit RDUController(QObject *parent = nullptr);
public slots:
    //Well understood events
    void injectTouch(QPoint l);
    void injectTouchRelease();
    void spinMainDial(int ticks);
    void setFrameDivisor(uint8_t ndivisor);

    //Debuging interfaces
    void writeWord(uint32_t addr, uint32_t data);
    void writeRequest(Request r);
    void writeInject(QByteArray toInject);
    void writeInjectHex(QString toInjectHex);


signals:
    void RDUReady(bool);
    void notifyUserOfState(QString);
    void notifyUserOfAction(QString);
    void newLrxdBytes(QByteArray);
    void newLtxdBytes(QByteArray);
protected:
    void timerEvent(QTimerEvent *event) override;
private slots:
    void readyRead();
private:
    void stepState();
    QTcpSocket socket;
    QByteArray msg_resp_buffer;
    int msg_resp_buffer_write = -1;
    int msg_resp_buffer_idx = 0;
    uint8_t divisor = 0;
    int m_dial_offset = 0;

    QMdnsEngine::Server mServer;
    QMdnsEngine::Resolver *mResolver = nullptr;
    QHostAddress m_addrAlt;

    enum state {
        RDU_Idle,
        RDU_Query_mDNS,
        RDU_Query_mDNS_Wait,
        RDU_ConnectRemote,
        RDU_ConnectRemote_Wait,
        RDU_DisableClock,
        RDU_DisableClock_Wait,
        RDU_SetupHostData,
        RDU_SetupHostData_Wait,
        RDU_SetFrameRate,
        RDU_SetFrameRate_Wait,
        RDU_EnableClock,
        RDU_EnableClock_Wait,
        RDU_Connected,
        RDU_Ping,
        RDU_Ping_Wait,
        RDU_ErrorState,
    };
    state idle();
    state Query_mDNS();
    state Query_mDNS_Wait();
    state ConnectRemote();
    state ConnectRemote_Wait();
    state DisableClock();
    state SetupHostData();
    state SetFrameRate();
    state EnableClock();
    state Connected();
    state Ping();
    state Error();

    state SpecialWaitAck(state remain, state acked);
    state nextState = RDU_Query_mDNS;
    state currentState = RDU_Idle;
    state previousState = RDU_Idle;
    QElapsedTimer timeInState;
    bool haveAck = false;
};

#endif // RDUCONTROLLER_H
