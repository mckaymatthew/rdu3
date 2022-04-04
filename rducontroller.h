#ifndef RDUCONTROLLER_H
#define RDUCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QHostInfo>
#include <QByteArray>
#include <QTcpSocket>
#include <QSslSocket>
#include "simple.pb.h"
#include <qmdnsengine/server.h>
#include <qmdnsengine/resolver.h>
#include <QElapsedTimer>
#include <QPoint>
#include <QSettings>


class RDUController : public QObject
{
    Q_OBJECT
public:

    enum Register {
        ctrl_reset = 0xf0000000,
        ctrl_halt = 0xf0000001, //Unaligned access causes soc to halt
        ctrl_scratch = 0xf0000004,
        ctrl_bus_errors = 0xf0000008,
        rgb_control_csr = 0xf0002800,
        rgb_frame_divisor_csr = 0xf0002804,
        rotary_main_dial_encoderOffset = 0xf0003000,
        rotary_multi_dial_encoderOffset = 0xf0003004,
        rotary_bpf_in_encoderOffset = 0xf0003008,
        rotary_bpf_out_encoderOffset = 0xf000300c,
        timer0_load = 0xf0005000,
        timer0_reload = 0xf0005004,
        timer0_en = 0xf0005008,
        timer0_update_value = 0xf000500c,
        timer0_value = 0xf0005010,
        timer0_ev_status = 0xf0005014,
        timer0_ev_pending = 0xf0005018,
        timer0_ev_enable = 0xf000501c,
        timer0_uptime_latch = 0xf0005020,
        timer0_uptime_cycles_msp = 0xf0005024,
        timer0_uptime_cycles_lsp = 0xf0005028, //Reg is 64 bits.
    };
    Q_ENUM(Register);

    explicit RDUController(QObject *parent = nullptr);
public slots:
    //Well understood events
    void injectTouch(QPoint l);
    void injectTouchRelease();
    void spinMultiDial(int ticks);
    void spinMainDial(int ticks);
    void spinBPFInDial(int ticks);
    void spinBPFOutDial(int ticks);
    void adjustVolume(uint8_t value);
    void adjustRfSql(uint8_t value);
    void setFrameDivisor(uint8_t ndivisor);

    //Debuging interfaces
    void writeWord(Register addr, uint32_t data);
    void writeRequest(Request r);
    void writeInjectStr(QString toInject);
    void writeInject(QByteArray toInject);
    void readWord(Register addr);


signals:
    void RDUReady(bool);
    void notifyUserOfState(QString);
    void notifyUserOfAction(QString);
    void newLrxdBytes(QByteArray);
    void newLtxdBytes(QByteArray);
    void readWordDone(Register addr, uint32_t data);
protected:
    void timerEvent(QTimerEvent *event) override;
private slots:
    void readyRead();
    void sslErrors(const QList<QSslError> &errors);
private:
    void stepState();
    QSslSocket socket;
    QByteArray msg_resp_buffer;
    int msg_resp_buffer_write = -1;
    int msg_resp_buffer_idx = 0;
    uint8_t divisor = 0;
    int m_main_dial_offset = 0;
    int m_multi_dial_offset = 0;
    int m_bpf_in_dial_offset = 0;
    int m_bpf_out_dial_offset = 0;

    QMdnsEngine::Server mServer;
    QMdnsEngine::Resolver *mResolver = nullptr;
    QHostAddress m_addrAlt;

    enum state {
        RDU_Idle,
        RDU_DecideLookupMethod,
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
        RDU_Read_MainDial,
        RDU_Read_MainDial_Wait,
        RDU_Read_MultiDial,
        RDU_Read_MultiDial_Wait,
        RDU_Read_BPFIn,
        RDU_Read_BPFIn_Wait,
        RDU_Read_BPFOut,
        RDU_Read_BPFOut_Wait,
        RDU_EnableClock,
        RDU_EnableClock_Wait,
        RDU_Connected,
        RDU_Ping,
        RDU_Ping_Wait,
        RDU_ErrorState,
    };
    state idle();
    state DecideLookupMethod();
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
    state ReadReg(Register reg, uint32_t *dst, state stateNext);

    state SpecialWaitAck(state remain, state acked);
    state nextState = RDU_Idle;
    state currentState = RDU_Idle;
    state previousState = RDU_Idle;
    QElapsedTimer timeInState;
    bool haveAck = false;
    uint32_t* regReadDestination = nullptr;
    QSettings m_settings;
};

#endif // RDUCONTROLLER_H
