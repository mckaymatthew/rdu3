#include "rducontroller.h"
#include <QSignalTransition>
#include "pb_encode.h"
#include "pb_decode.h"
#include "csrmap.h"

RDUController::RDUController(QObject *parent)
    : QObject(parent)
    , socket()
    , msg_resp_buffer(Response_size,'\0')
    , msg_resp_buffer_write(-1)
    , msg_resp_buffer_idx(0)

{
    connect(&socket, &QTcpSocket::readyRead,this, &RDUController::readyRead);
    startTimer(100);
}
void RDUController::timerEvent(QTimerEvent* ) {
    stepState();
}

void RDUController::stepState() {
    if(nextState != currentState) {
        previousState = currentState;
        currentState = nextState;
    }
    switch(currentState) {
    case RDU_Idle:
        nextState = idle();
        break;
    case RDU_Query_mDNS:
        nextState = Query_mDNS();
        break;
    case RDU_Query_mDNS_Wait:
        nextState = Query_mDNS_Wait();
        break;
    case RDU_ConnectRemote:
        nextState = ConnectRemote();
        break;
    case RDU_ConnectRemote_Wait:
        nextState = ConnectRemote_Wait();
        break;
    case RDU_DisableClock:
        nextState = DisableClock();
        break;
    case RDU_DisableClock_Wait:
        nextState = SpecialWaitAck(currentState, RDU_SetupHostData);
        break;
    case RDU_SetupHostData:
        nextState = SetupHostData();
        break;
    case RDU_SetupHostData_Wait:
        nextState = SpecialWaitAck(currentState, RDU_SetFrameRate);
        break;
    case RDU_SetFrameRate:
        nextState = SetFrameRate();
        break;
    case RDU_SetFrameRate_Wait:
        nextState = SpecialWaitAck(currentState, RDU_EnableClock);
        break;
    case RDU_EnableClock:
        nextState = EnableClock();
        break;
    case RDU_EnableClock_Wait:
        nextState = SpecialWaitAck(currentState, RDU_Connected);
        break;
    case RDU_Connected:
        nextState = Connected();
        break;
    case RDU_Ping:
        nextState = Ping();
        break;
    case RDU_Ping_Wait:
        nextState = SpecialWaitAck(currentState, RDU_Connected);
        break;
    case RDU_ErrorState:
        nextState = Error();
        break;
    }
    haveAck = false;
    if(nextState != currentState) {
        timeInState.restart();
    }
}

RDUController::state RDUController::idle() {
    emit notifyUserOfState(QString("Idle"));
    return RDUController::state::RDU_Query_mDNS;
}

RDUController::state RDUController::Query_mDNS() {
    if(mResolver != nullptr) {
        delete mResolver;
        mResolver = nullptr;
    }
    m_addrAlt.clear();

    emit notifyUserOfState(QString("Query mDNS for IC7300"));
    qInfo() << ("Query network for IC-7300 RDU...");
    mResolver = new QMdnsEngine::Resolver(&mServer, "rdu_ic7300.local.", nullptr, this);
    connect(mResolver, &QMdnsEngine::Resolver::resolved, [this](const QHostAddress &address) {
        qInfo() << QString("Resolver Found: %1").arg(address.toString());
        m_addrAlt = address;
    });
    return RDUController::state::RDU_Query_mDNS_Wait;
}

RDUController::state RDUController::Query_mDNS_Wait() {
    const bool queryOk = !m_addrAlt.isNull();
    const bool queryTimeout = timeInState.elapsed() > 5000;
    if(queryOk) {
        return RDUController::state::RDU_ConnectRemote;
    } else if(queryTimeout) {
        emit notifyUserOfState(QString("mDNS Query Time Out"));
        qInfo() << (QString("mDNS query timed out, was at %1").arg(timeInState.elapsed()));
        return RDUController::state::RDU_Idle;
    }
    return RDUController::state::RDU_Query_mDNS_Wait;
}

RDUController::state RDUController::ConnectRemote() {
    emit notifyUserOfState(QString("Located IC7300, Connecting"));
    qInfo() << (QString("Found RDU at %1, connecting...").arg(m_addrAlt.toString()));
    socket.connectToHost(m_addrAlt, 4242);
    return RDUController::state::RDU_ConnectRemote_Wait;
}
RDUController::state RDUController::ConnectRemote_Wait() {
    const bool connected = socket.state() == QAbstractSocket::ConnectedState;
    const bool timeout = timeInState.elapsed() > 5000;
    if(connected) {
        return RDUController::state::RDU_DisableClock;
    } else if(timeout) {
        emit notifyUserOfState(QString("Connect Time Out"));
        qInfo() << (QString("Connect timed out, was at %1").arg(timeInState.elapsed()));
        socket.close();
        socket.abort();
        return RDUController::state::RDU_Idle;
    }
    return RDUController::state::RDU_ConnectRemote_Wait;
}

RDUController::state RDUController::DisableClock() {
    emit notifyUserOfState(QString("Stop FPGA"));
    qInfo() << ("LCD Clock Inhibit.");
    writeWord(CSRMap::get().CLK_GATE,0);
    return RDUController::state::RDU_DisableClock_Wait;
}
RDUController::state RDUController::SetupHostData() {
    emit notifyUserOfState(QString("Set FPGA Transmit Networking"));
    qInfo() << ("Setup RDU FPGA Buffers.");
    Request r = Request_init_default;
    r.which_payload = Request_setupSlots_tag;
    writeRequest(r);
    return RDUController::state::RDU_SetupHostData_Wait;
}
RDUController::state RDUController::SetFrameRate() {
    emit notifyUserOfState(QString("Set FPGA FPS"));
    qInfo() << (QString("Set FPS Divisor %1.").arg(divisor));
    writeWord(CSRMap::get().FPS_DIVISOR,divisor);
    return RDUController::state::RDU_SetFrameRate_Wait;
}
RDUController::state RDUController::EnableClock() {
    emit notifyUserOfState(QString("Activate FPGA Transmit"));
    qInfo() << ("LCD Clock Enable.");
    writeWord(CSRMap::get().CLK_GATE,1);
    return RDUController::state::RDU_EnableClock_Wait;
}
RDUController::state RDUController::Connected() {
    emit notifyUserOfState(QString("Connected"));
    emit RDUReady();
    const bool timeToPing = timeInState.elapsed() > 5000;
    if(timeToPing) {
        return RDUController::state::RDU_Ping;
    }
    return RDUController::state::RDU_Connected;
}

RDUController::state RDUController::Ping() {
    emit notifyUserOfState(QString("Ping IC7300"));
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
    return RDUController::state::RDU_Ping_Wait;
}


RDUController::state RDUController::Error() {
    emit notifyUserOfState(QString("Error - Disconnect"));
    qInfo() << (QString("Error occured. Socket state: %1, errors: %2").arg(socket.state()).arg(socket.errorString()));
    if(socket.isOpen()) {
        socket.close();
    }
    if(socket.state() != 0) {
        socket.abort();
    }
    return RDUController::state::RDU_Idle;
}

RDUController::state RDUController::SpecialWaitAck(state waiting, state success) {
    const bool timeout = timeInState.elapsed() > 5000;
    if(timeout) {
        emit notifyUserOfState(QString("Timeout waiting for Ack"));
        qInfo() << (QString("Ack timed out, was at %1. Waiting: %2, Success %3").arg(timeInState.elapsed()).arg(waiting).arg(success));
        return RDUController::state::RDU_ErrorState;
    } else if(haveAck) {
        qInfo() << (QString("Got ack, was at %1. Waiting: %2, Success %3").arg(timeInState.elapsed()).arg(waiting).arg(success));
        return success;
    } else {
        return waiting;
    }
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
                        haveAck = true;
                    }
                    if(msg_resp.which_payload == Response_ack_tag) {
                        haveAck = true;
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
