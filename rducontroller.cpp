#include "rducontroller.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "RDUConstants.h"
#include <QtEndian>

RDUController::RDUController(QObject *parent)
    : QObject(parent)
    , socket()
    , msg_resp_buffer(Response_size,'\0')
    , msg_resp_buffer_write(-1)
    , msg_resp_buffer_idx(0)
    , m_settings("KE0PSL", "RDU3")

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
    case RDU_DecideLookupMethod:
        nextState = DecideLookupMethod();
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
        nextState = SpecialWaitAck(currentState, RDU_Read_MainDial);
        break;
    case RDU_Read_MainDial:
        nextState = ReadReg(rotary_main_dial_encoderOffset, (uint32_t*)&m_main_dial_offset, RDU_Read_MainDial_Wait);
        break;
    case RDU_Read_MainDial_Wait:
        nextState = SpecialWaitAck(currentState, RDU_Read_MultiDial);
        break;
    case RDU_Read_MultiDial:
        nextState = ReadReg(rotary_multi_dial_encoderOffset, (uint32_t*)&m_multi_dial_offset, RDU_Read_MultiDial_Wait);
        break;
    case RDU_Read_MultiDial_Wait:
        nextState = SpecialWaitAck(currentState, RDU_Read_BPFIn);
        break;
    case RDU_Read_BPFIn:
        nextState = ReadReg(rotary_bpf_in_encoderOffset, (uint32_t*)&m_bpf_in_dial_offset, RDU_Read_BPFIn_Wait);
        break;
    case RDU_Read_BPFIn_Wait:
        nextState = SpecialWaitAck(currentState, RDU_Read_BPFOut);
        break;
    case RDU_Read_BPFOut:
        nextState = ReadReg(rotary_bpf_out_encoderOffset, (uint32_t*)&m_bpf_out_dial_offset, RDU_Read_BPFOut_Wait);
        break;
    case RDU_Read_BPFOut_Wait:
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
    if(haveAck) {
        qInfo() << (QString("Clear Ack"));
    }
    haveAck = false;
    if(nextState != currentState) {
        timeInState.restart();
    }
}

RDUController::state RDUController::idle() {
    emit notifyUserOfState(QString("Idle"));
    emit RDUReady(false);
    return RDUController::state::RDU_DecideLookupMethod;
}

RDUController::state RDUController::DecideLookupMethod() {
    auto deviceIp = this->m_settings.value("deviceIp","0.0.0.0").toString();
    m_addrAlt = QHostAddress(deviceIp);
    if(m_addrAlt == QHostAddress::AnyIPv4) {
        return RDUController::state::RDU_Query_mDNS;
    } else {
        return RDUController::state::RDU_ConnectRemote;
    }

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
    emit notifyUserOfState(QString("Connecting to %1").arg(m_addrAlt.toString()));
    qInfo() << (QString("RDU is at %1, connecting...").arg(m_addrAlt.toString()));
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
    writeWord(rgb_control_csr,0);
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
    writeWord(rgb_frame_divisor_csr,divisor);
    return RDUController::state::RDU_SetFrameRate_Wait;
}
RDUController::state RDUController::EnableClock() {
    emit notifyUserOfState(QString("Activate FPGA Transmit"));
    writeWord(rgb_control_csr,1);
    return RDUController::state::RDU_EnableClock_Wait;
}
RDUController::state RDUController::Connected() {
    emit notifyUserOfState(QString("Connected"));
    emit RDUReady(true);
    const bool timeToPing = timeInState.elapsed() > 5000;
    if(timeToPing) {
        return RDUController::state::RDU_Ping;
    }
    return RDUController::state::RDU_Connected;
}

RDUController::state RDUController::Ping() {
    //Too chatty to print every ping.
//    emit notifyUserOfState(QString("Ping IC7300"));
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
    socket.write((const char *)buffer,message_length);
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
//        qInfo() << (QString("Got ack, was at %1. Waiting: %2, Success %3").arg(timeInState.elapsed()).arg(waiting).arg(success));
        return success;
    } else {
        return waiting;
    }
}

RDUController::state RDUController::ReadReg(Register addr, uint32_t *dst, RDUController::state stateNext) {
    Request r = Request_init_default;
    r.which_payload = Request_readWord_tag;
    r.payload.readWord.address = addr;
    regReadDestination = dst;

    auto regName = QVariant::fromValue(addr).toString();
    emit notifyUserOfState(QString("Read register %1").arg(regName));
    writeRequest(r);
    return stateNext;
}

void RDUController::readWord(Register addr) {
    Request r = Request_init_default;
    r.which_payload = Request_readWord_tag;
    r.payload.readWord.address = addr;
    regReadDestination = nullptr;

    auto regName = QVariant::fromValue(addr).toString();
    emit notifyUserOfState(QString("Read register %1").arg(regName));
    writeRequest(r);
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
                        qInfo() << (QString("Got Ping/Ack"));
                    }
                    if(msg_resp.which_payload == Response_ack_tag) {
                        haveAck = true;
                        qInfo() << (QString("Ack"));
                    }
                    if(msg_resp.which_payload == Response_readWord_tag) {
                        if(msg_resp.payload.readWord.has_data) {
                            qInfo() << (QString("Read Reponse: %1.").arg(msg_resp.payload.readWord.data,8,16,QChar('0')));

                            if(regReadDestination != nullptr) {
                                *regReadDestination = msg_resp.payload.readWord.data;
                            } else {
                                emit readWordDone((RDUController::Register)msg_resp.payload.readWord.address, msg_resp.payload.readWord.data);
                            }
                            haveAck = true;
                        } else {
                            qWarning() << QString("Got unexpected empty read word response for %1").arg(msg_resp.payload.readWord.address,8,16,QChar('0'));
                        }
                    }
                    if(msg_resp.which_payload == Response_lrxd_tag) {
                        QByteArray lrxd((char *)msg_resp.payload.lrxd.data.bytes, msg_resp.payload.lrxd.data.size);
                        emit newLrxdBytes(lrxd);
//                        qInfo() << (QString("Got LRXD Bytes: %1 %2").arg(lrxd.size()).arg(QString(lrxd.toHex())));
                    }
                    if(msg_resp.which_payload == Response_ltxd_tag) {
                        QByteArray ltxd((char *)msg_resp.payload.ltxd.data.bytes, msg_resp.payload.ltxd.data.size);
                        emit newLtxdBytes(ltxd);
                    }
                }
            }
        }
    }
}

void RDUController::writeInjectStr(QString toInject) {
    writeInject(QByteArray::fromHex(toInject.toUtf8()));
}
void RDUController::writeInject(QByteArray toInject) {
    Request r = Request_init_default;
    qsizetype maxInject = sizeof(r.payload.inject.data.bytes);
    if(toInject.size() > maxInject) {
        qWarning() << (QString("Inject request too big.  %1.").arg(toInject.size()));
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
        QByteArray outBuff((qsizetype)(Request_size+1), '\0');
        pb_ostream_t stream = pb_ostream_from_buffer((pb_byte_t *)(outBuff.data() + 1), outBuff.size() - 1);
        pb_encode(&stream, Request_fields, &r);
        outBuff[0] = stream.bytes_written;
        outBuff.truncate(stream.bytes_written+1);
        socket.write(outBuff);
        socket.flush();
    }
}
void RDUController::writeWord(Register addr, uint32_t data) {
    Request r = Request_init_default;
    r.which_payload = Request_writeWord_tag;
    r.payload.writeWord.address = addr;
    r.payload.writeWord.has_data = true;
    r.payload.writeWord.data = data;

    auto regName = QVariant::fromValue(addr).toString();
    qInfo() << QString("Write 0x%1 to %2.").arg(data,8,16,QChar('0')).arg(regName);
    writeRequest(r);
}

void RDUController::setFrameDivisor(uint8_t ndivisior) {
    this->divisor = ndivisior;
    writeWord(rgb_frame_divisor_csr,ndivisior);
}

void RDUController::spinMultiDial(int ticks) {
    m_multi_dial_offset = m_multi_dial_offset + ticks;
    emit notifyUserOfAction(QString("Multi Dial %1 (abs: %2)").arg(ticks).arg(m_multi_dial_offset));
    if(socket.isValid()) {
        writeWord(rotary_multi_dial_encoderOffset,m_multi_dial_offset);
    }

}
void RDUController::spinMainDial(int ticks) {
    m_main_dial_offset = m_main_dial_offset + ticks;
    emit notifyUserOfAction(QString("Main Dial %1 (abs: %2)").arg(ticks).arg(m_main_dial_offset));
    if(socket.isValid()) {
        writeWord(rotary_main_dial_encoderOffset,m_main_dial_offset);
    }
}
void RDUController::spinBPFInDial(int ticks) {
    m_bpf_in_dial_offset = m_bpf_in_dial_offset + ticks;
    emit notifyUserOfAction(QString("BPF Inner Dial %1 (abs: %2)").arg(ticks).arg(m_bpf_in_dial_offset));
    if(socket.isValid()) {
        writeWord(rotary_bpf_in_encoderOffset,m_bpf_in_dial_offset);
    }
}
void RDUController::spinBPFOutDial(int ticks) {
    m_bpf_out_dial_offset = m_bpf_out_dial_offset + ticks;
    emit notifyUserOfAction(QString("BPF Outer Dial %1 (abs: %2)").arg(ticks).arg(m_bpf_out_dial_offset));
    if(socket.isValid()) {
        writeWord(rotary_bpf_out_encoderOffset,m_bpf_out_dial_offset);
    }
}

void RDUController::adjustVolume(uint8_t value) {
    emit notifyUserOfAction(QString("Volume Set %1").arg(value));
    QByteArray injectParameter = QByteArray::fromHex("fe1e00fd");
    injectParameter[2] = value;
    writeInject(injectParameter);
}
void RDUController::adjustRfSql(uint8_t value) {
    emit notifyUserOfAction(QString("AfSql Set %1").arg(value));
    QByteArray injectParameter = QByteArray::fromHex("fe1f00fd");
    injectParameter[2] = value;
    writeInject(injectParameter);
}


void RDUController::injectTouch(QPoint l) {
    if(l.x() < 0 || l.x() > 480) {
        return;
    }
    if(l.y() < 0 || l.y() > 272) {
        return;
    }
    emit notifyUserOfAction(QString("Touch Point: %1,%2").arg(l.x()).arg(l.y()));
    quint16 x = qToBigEndian<quint16>(l.x());
    quint16 y = qToBigEndian<quint16>(l.y());
    QByteArray injectParameter = QByteArray::fromHex("fe1300");
    injectParameter.append(quint8(x>>0));
    injectParameter.append(quint8(x>>8));
    injectParameter.append(quint8(y>>0));
    injectParameter.append(quint8(y>>8));
    injectParameter.append(QByteArray::fromHex("fd"));
    qInfo() << (QString("Inject touch to %1,%2. Hex: %3 ").arg(l.x()).arg(l.y()).arg(QString(injectParameter.toHex())));
    writeInject(injectParameter);

}
void RDUController::injectTouchRelease() {
    emit notifyUserOfAction(QString("Touch Release"));
    QByteArray injectParameter = QByteArray::fromHex("fe1300270f270ffd");
    qInfo() << (QString("Touch Release: %1.").arg(QString(injectParameter.toHex())));
    writeInject(injectParameter);
}
