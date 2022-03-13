#include "rducontroller.h"
#include <QSignalTransition>
#include "pb_encode.h"
#include "pb_decode.h"
#include "csrmap.h"

RDUController::RDUController(QObject *parent)
    : QObject(parent)
    , machine()
    , mdnsQueryTimeout()
    , rduHost()
    , socket()
    , periodicPing()
    , pingTimeout()
    , setupTimeout()
    , msg_resp_buffer(Response_size,'\0')
    , msg_resp_buffer_write(-1)
    , msg_resp_buffer_idx(0)

{
    connect(&socket, &QTcpSocket::readyRead,this, &RDUController::readyRead);
    setupStateMachine();
}

void RDUController::startController() {
    machine.start();
}
void RDUController::setupStateMachine()
{
    QState *s_errorRestart = new QState();
    QState *s_queryMDNS = new QState();
    QState *s_connectToRdu = new QState();
    QState *s_connected = new QState();
    QState *s_disablePixelClock = new QState();
    QState *s_setupHostData = new QState();
    QState *s_enablePixelClock = new QState();
    QState *s_setFrameRate = new QState();
    QState *s_ping = new QState();

    machine.addState(s_errorRestart);
    machine.addState(s_queryMDNS);
    machine.addState(s_connectToRdu);
    machine.addState(s_connected);
    machine.addState(s_disablePixelClock);
    machine.addState(s_setFrameRate);
    machine.addState(s_setupHostData);
    machine.addState(s_enablePixelClock);

    machine.addState(s_ping);
    machine.setInitialState(s_queryMDNS);

    //Nominal program flow
    s_errorRestart->addTransition(s_queryMDNS);
    s_queryMDNS->addTransition(this,&RDUController::foundHost, s_connectToRdu);
    auto t_connectedTransition = s_connectToRdu->addTransition(&socket,&QTcpSocket::connected, s_disablePixelClock);
    s_disablePixelClock->addTransition(this,&RDUController::gotAck,s_setFrameRate);
    s_setFrameRate->addTransition(this,&RDUController::gotAck,s_setupHostData);
    s_setupHostData->addTransition(this,&RDUController::gotAck,s_enablePixelClock);
    s_enablePixelClock->addTransition(this,&RDUController::gotAck,s_connected);

    s_connected->addTransition(&periodicPing,&QTimer::timeout, s_ping);
    s_ping->addTransition(this,&RDUController::pingResponse, s_connected);

    auto setupStates = {s_enablePixelClock, s_setupHostData, s_disablePixelClock, s_setFrameRate};
    auto socketStates = {s_connectToRdu, s_connected, s_ping, s_disablePixelClock, s_setFrameRate, s_setupHostData, s_enablePixelClock};
    auto notReadyStates = {s_errorRestart, s_queryMDNS,s_connectToRdu};
    auto readyStates = {s_connected, s_disablePixelClock, s_setupHostData, s_enablePixelClock, s_ping, s_setFrameRate};
    for(auto state: setupStates) {
        auto t_timeoutTransition = state->addTransition(&setupTimeout, &QTimer::timeout,s_errorRestart);
        connect(t_timeoutTransition, &QSignalTransition::triggered, this, &RDUController::notifyTimeout);
    }
    for(auto s: socketStates) {
        //Todo: Are there other socket errors that can occur?
        s->addTransition(&socket,&QTcpSocket::disconnected, s_errorRestart);
    }
    for(auto s: notReadyStates) {
        connect(s, &QState::entered, [this](){emit this->RDUNotReady();});
    }
    for(auto s: readyStates) {
        connect(s, &QState::entered, [this](){emit this->RDUReady();});
    }


    //Timeout for operations
    auto t_connectTimeout = s_connectToRdu->addTransition(&connectTimeout,&QTimer::timeout, s_errorRestart);
    auto t_mdnsTimeout = s_queryMDNS->addTransition(&mdnsQueryTimeout,&QTimer::timeout,s_queryMDNS);
    auto t_pingTimeout = s_ping->addTransition(&pingTimeout,&QTimer::timeout,s_errorRestart);
    auto t_fpsTimeout = s_setFrameRate->addTransition(&fpsTimeout,&QTimer::timeout,s_errorRestart);
    connect(t_fpsTimeout, &QSignalTransition::triggered, this, &RDUController::notifyFPSTimeout);
    connect(t_pingTimeout, &QSignalTransition::triggered, this, &RDUController::notifyPingTimeout);
    connect(s_errorRestart, &QState::entered, this, &RDUController::notifySocketError);

    connect(s_queryMDNS, &QState::entered, this, &RDUController::doQueryMdns);
    connect(t_mdnsTimeout,&QSignalTransition::triggered, this, &RDUController::notifyMdnsTimeout);
    connect(t_connectTimeout,&QSignalTransition::triggered, this, &RDUController::notifyConnectTimeout);
    connect(s_connectToRdu, &QState::entered, this, &RDUController::doConnectToRdu);
    connect(t_connectedTransition, &QSignalTransition::triggered, this, &RDUController::notifyConnected);

    connect(s_ping, &QState::entered, this, &RDUController::doPing);

    connect(s_disablePixelClock, &QState::entered, this, &RDUController::doClkInhibit);
    connect(s_setupHostData, &QState::entered, this, &RDUController::doSetup);
    connect(s_enablePixelClock, &QState::entered, this, &RDUController::doClkEnable);
    connect(s_setFrameRate, &QState::entered, this, &RDUController::doSetFps);


}
void RDUController::notifySocketError() {
    emit notifyUserOfState(QString("Error"));
    qInfo() << (QString("Error occured. Socket state: %1, errors: %2").arg(socket.state()).arg(socket.errorString()));
    if(socket.isOpen()) {
        socket.close();
    }
    if(socket.state() != 0) {
        socket.abort();
    }
    //qInfo() << (QString("Error occured. Socket state: %1, errors: %2").arg(socket.state()).arg(socket.errorString()));

}
void RDUController::notifyFPSTimeout(){
    emit notifyUserOfState(QString("Timeout Set FPS"));
    qInfo() << ("RDU did not respond to Set FPS in time.");
}
void RDUController::notifyPingTimeout() {
    emit notifyUserOfState(QString("Timeout Ping"));
    qInfo() << ("RDU did not respond to ping in time.");
}
void RDUController::notifyTimeout() {
    emit notifyUserOfState(QString("Timeout Configure"));
    qInfo() << ("Failed to setup RDU configuration.");
}
void RDUController::doQueryMdns() {
    emit notifyUserOfState(QString("Query mDNS for IC7300"));
    qInfo() << ("Query network for IC-7300 RDU...");
//    m_mDNS->lookup("rdu_ic7300.local");
    if(mResolver != nullptr) {
        delete mResolver;
    }
    mResolver = new QMdnsEngine::Resolver(&mServer, "rdu_ic7300.local.", nullptr, this);
    connect(mResolver, &QMdnsEngine::Resolver::resolved, [this](const QHostAddress &address) {
        m_addrAlt = address;
        emit this->foundHost();
    });
    this->mdnsQueryTimeout.setSingleShot(true);
    this->mdnsQueryTimeout.start(5000);
};
void RDUController::notifyMdnsTimeout() {
    emit notifyUserOfState(QString("Query mDNS Timeout"));
    qInfo() << ("Failed to find IC7300 RDU on network");
};

void RDUController::doConnectToRdu() {
    emit notifyUserOfState(QString("Located IC7300, Connecting"));
//    auto addr = rduHost.addresses().first();

//    QHostAddress addr = QHostAddress("10.0.0.128");
    qInfo() << (QString("Found RDU at %1, connecting...").arg(m_addrAlt.toString()));
    socket.connectToHost(m_addrAlt, 4242);
    connectTimeout.start(1500);
}

void RDUController::notifyConnectTimeout() {
    emit notifyUserOfState(QString("Timeout Connect"));
    qInfo() << ("Failed to connect.");
}
void RDUController::notifyConnected() {
    emit notifyUserOfState(QString("Connected"));
    qInfo() << ("Connected.");
    emit RDUReady();
    periodicPing.start(5000);
}

void RDUController::doClkInhibit() {
    emit notifyUserOfState(QString("Setup Clk"));
    qInfo() << ("LCD Clock Inhibit.");
    writeWord(CSRMap::get().CLK_GATE,0);
    setupTimeout.setSingleShot(true);
    setupTimeout.start(5000);
}
void RDUController::doSetup() {
    emit notifyUserOfState(QString("Setup IP"));
    qInfo() << ("Setup RDU FPGA Buffers.");
    Request r = Request_init_default;
    r.which_payload = Request_setupSlots_tag;
    writeRequest(r);
    setupTimeout.setSingleShot(true);
    setupTimeout.start(5000);
}
void RDUController::doClkEnable() {
    emit notifyUserOfState(QString("Setup En"));
    qInfo() << ("LCD Clock Enable.");
    writeWord(CSRMap::get().CLK_GATE,1);
    setupTimeout.setSingleShot(true);
    setupTimeout.start(5000);
    emit notifyUserOfState(QString("Connected"));
}
void RDUController::doSetFps() {
    emit notifyUserOfState(QString("Setup FPS"));
    qInfo() << (QString("Set FPS Divisor %1.").arg(divisor));
    writeWord(CSRMap::get().FPS_DIVISOR,divisor);
    fpsTimeout.setSingleShot(true);
    fpsTimeout.start(5000);
}

void RDUController::doPing() {
    emit notifyUserOfState(QString("Connected"));
    //updateState("Ping Sent");
    Request r = Request_init_default;
    r.which_payload = Request_ping_tag;
    r.payload.ping.magic[0] = 0xFE;
    r.payload.ping.magic[1] = 0xED;
    r.payload.ping.magic[2] = 0xBE;
    r.payload.ping.magic[3] = 0xEF;

    uint8_t buffer[Request_size];

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    pb_encode(&stream, Request_fields, &r);
    uint8_t message_length = stream.bytes_written;
    socket.write((const char *)&message_length,1);
    auto writeRet = socket.write((const char *)buffer,message_length);
    pingTimeout.setSingleShot(true);
    pingTimeout.start(5000);
}

void RDUController::readyRead() {
    auto data = socket.readAll();
    for(int i = 0; i < data.size(); i++) {
        if(msg_resp_buffer_write == -1) {
            msg_resp_buffer_write = data[i];
//                this->ui->statusMessages->append(QString("Start of response size %1 bytes.").arg(msg_resp_buffer_write));
        } else {
//                qDebug() << QString("msg_resp_buffer.size()  %1 [Response_size %5], msg_resp_buffer_idx %2, data.size() %3, i %4").arg(msg_resp_buffer.size()).arg(msg_resp_buffer_idx).arg(data.size()).arg(i).arg(Response_size);
            msg_resp_buffer[msg_resp_buffer_idx++] = data[i];
            if(msg_resp_buffer_idx == msg_resp_buffer_write) {
//                    this->ui->statusMessages->append(QString("End of request size %1 bytes.").arg(msg_resp_buffer_write));

                Response msg_resp = Response_init_default;
                pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)msg_resp_buffer.data(), msg_resp_buffer_write);

                msg_resp_buffer_write = -1;
                msg_resp_buffer_idx = 0;
                int status = pb_decode(&stream, Response_fields, &msg_resp);

                /* Check for errors... */
                if (!status)
                {
                    qInfo() << (QString("Error occurred during decode: %1.").arg(PB_GET_ERROR(&stream)));
                } else {
                    if(msg_resp.which_payload == Response_ping_tag) {
                        emit pingResponse();
                    }
                    if(msg_resp.which_payload == Response_ack_tag) {
                        emit gotAck();
                    }
                    if(msg_resp.which_payload == Response_lrxd_tag) {
                        QByteArray lrxd((char *)msg_resp.payload.lrxd.data.bytes, msg_resp.payload.lrxd.data.size);
                        qInfo() << (QString("Got LRXD Bytes: %1 %2").arg(lrxd.size()).arg(lrxd.toHex()));
                    }
                    if(msg_resp.which_payload == Response_ltxd_tag) {
                        QByteArray ltxd((char *)msg_resp.payload.ltxd.data.bytes, msg_resp.payload.ltxd.data.size);
                        m_ltxd_decoder.newData(ltxd);
                    }
                }
            }
        }
    }
}
void RDUController::writeInjectHex(QString toInjectHex) {
    writeInject(QByteArray::fromHex(toInjectHex.toLatin1()));
}
void RDUController::writeInject(QByteArray toInject) {
    Request r = Request_init_default;
    if(toInject.size() > sizeof(r.payload.inject.data.bytes)) {
        qInfo() << (QString("Inject request too big.  %1.").arg(toInject.size()));
        return;
    }
    r.which_payload = Request_inject_tag;
    r.payload.inject.data.size = toInject.size();
    for(int i = 0; i < toInject.size(); i++) {
        r.payload.inject.data.bytes[i] = toInject[i];
    }
    writeRequest(r);
}
void RDUController::writeRequest(Request r) {
    if(socket.isValid()) {
        uint8_t buffer[Request_size];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        pb_encode(&stream, Request_fields, &r);
        uint8_t message_length = stream.bytes_written;
        socket.write((const char *)&message_length,1);
        auto writeRet = socket.write((const char *)buffer,message_length);
        socket.flush();
    }

}
void RDUController::writeWord(uint32_t addr, uint32_t data) {
    Request r = Request_init_default;
    r.which_payload = Request_writeWord_tag;
    r.payload.writeWord.address = addr;
    r.payload.writeWord.data = data;

    qInfo() << (QString("Write 0x%1 to 0x%2.").arg(addr,8,16,QChar('0')).arg(data,8,16,QChar('0')));
    writeRequest(r);
}
void RDUController::setFrameDivisor(uint8_t ndivisior) {
    this->divisor = ndivisior;
    if(socket.isValid()) {
        writeWord(CSRMap::get().FPS_DIVISOR,ndivisior);
    }
}

void RDUController::spinMainDial(int ticks) {
    m_dial_offset = m_dial_offset + ticks;
    if(socket.isValid()) {
        writeWord(CSRMap::get().MAIN_DAIL_OFFSET,m_dial_offset);
    }

}
