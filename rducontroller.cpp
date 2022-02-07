#include "rducontroller.h"
#include <QSignalTransition>ß
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
    , msg_resp_buffer(' ',Response_size)
    , msg_resp_buffer_write(-1)
    , msg_resp_buffer_idx(0)
    , m_mDNS(qMDNS::getInstance())

{

    connect(&socket, &QTcpSocket::readyRead,this, &RDUController::readyRead);
    setupStateMachine();

    machine.start();
}

void RDUController::setupStateMachine()
{

    QState *errorRestart = new QState();
    QState *queryMDNS = new QState();
    QState *connectToRdu = new QState();
    QState *connected = new QState();
    QState *disablePixelClock = new QState();
    QState *setupHostData = new QState();
    QState *enablePixelClock = new QState();
    QState *ping = new QState();

//    m_states.append(QSharedPointer<QState>(errorRestart));
//    m_states.append(QSharedPointer<QState>(queryMDNS));
//    m_states.append(QSharedPointer<QState>(connectToRdu));
//    m_states.append(QSharedPointer<QState>(connected));
//    m_states.append(QSharedPointer<QState>(disablePixelClock));
//    m_states.append(QSharedPointer<QState>(setupHostData));
//    m_states.append(QSharedPointer<QState>(enablePixelClock));
//    m_states.append(QSharedPointer<QState>(ping));

    machine.addState(errorRestart);
    machine.addState(queryMDNS);
    machine.addState(connectToRdu);
    machine.addState(connected);
    machine.addState(disablePixelClock);
    machine.addState(setupHostData);
    machine.addState(enablePixelClock);

    machine.addState(ping);
    machine.setInitialState(connectToRdu);



    //Nominal program flow
    errorRestart->addTransition(queryMDNS);
    queryMDNS->addTransition(this,&RDUController::foundHost, connectToRdu);
    auto connectedTransition = connectToRdu->addTransition(&socket,&QTcpSocket::connected, disablePixelClock);
    disablePixelClock->addTransition(this,&RDUController::gotAck,setupHostData);
    setupHostData->addTransition(this,&RDUController::gotAck,enablePixelClock);
    enablePixelClock->addTransition(this,&RDUController::gotAck,connected);
    connected->addTransition(&periodicPing,&QTimer::timeout, ping);
    ping->addTransition(this,&RDUController::pingResponse, connected);
\
    auto setupStates = {enablePixelClock, setupHostData, disablePixelClock};
    for(auto state: setupStates) {
        connect(state->addTransition(&setupTimeout, &QTimer::timeout,errorRestart), &QSignalTransition::triggered, [&]() {
            emit logMessage("Time out during RDU setup.");
        });
    }

    //Timeout for operations
    auto dnsTimeoutTransition = queryMDNS->addTransition(&mdnsQueryTimeout,&QTimer::timeout,queryMDNS);
    auto pingTimeoutTransition = ping->addTransition(&pingTimeout,&QTimer::timeout,errorRestart);


    //Socket errors.
    auto socketStates = {connectToRdu,connected,ping,disablePixelClock, setupHostData, enablePixelClock};
    for(auto s: socketStates) {
//        s->addTransition(&socket,SIGNAL(error(QAbstractSocket::SocketError)), errorRestart);
        s->addTransition(&socket,&QTcpSocket::disconnected, errorRestart);
    }

    connect(errorRestart, &QState::entered, [this](){
        emit logMessage(QString("Error occured. Socket state: %1.").arg(socket.errorString()));
        if(socket.isOpen()) {
            socket.close();
        }
    });

    connect(queryMDNS, &QState::entered, [this](){
        emit logMessage("Query network for IC-7300 RDU");
        m_mDNS->lookup("rdu_ic7300.local");

        this->mdnsQueryTimeout.setSingleShot(true);
        this->mdnsQueryTimeout.start(5000);
    });
    connect(dnsTimeoutTransition,&QSignalTransition::triggered, [this](){
        emit logMessage("Lookup time out");
    });
    connect(connectToRdu, &QState::entered, [this](){
//        auto addr = rduHost.addresses().first();
        QHostAddress addr = QHostAddress("10.0.0.128");
        emit logMessage("Connect to IC-7300 RDU...");
        socket.connectToHost(addr, 4242);
    });
    connect(connectedTransition, &QSignalTransition::triggered, [this](){
        emit logMessage("Connected.");
        periodicPing.start(1000);
    });
    connect(ping, &QState::entered, [this](){
//        updateState("Ping Sent");
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
    });
    connect(pingTimeoutTransition, &QSignalTransition::triggered, [this](){
        emit logMessage("Lost connection or device unresponsive.");
    });

    connect(disablePixelClock, &QState::entered, [this](){
        emit logMessage("LCD Clock Inhibit.");
        writeWord(CSRMap::get().CLK_GATE,0);
        setupTimeout.setSingleShot(true);
        setupTimeout.start(5000);
    });
    connect(setupHostData, &QState::entered, [this](){
        emit logMessage("Setup RDU FPGA Buffers.");
        Request r = Request_init_default;
        r.which_payload = Request_setupSlots_tag;
        writeRequest(r);
        setupTimeout.setSingleShot(true);
        setupTimeout.start(5000);
    });
    connect(enablePixelClock, &QState::entered, [this](){
        emit logMessage("LCD Clock Enable.");
        writeWord(CSRMap::get().CLK_GATE,1);
        setupTimeout.setSingleShot(true);
        setupTimeout.start(5000);
    });

}

void RDUController::readyRead() {
    auto data = socket.readAll();
    for(int i = 0; i < data.size(); i++) {
        if(msg_resp_buffer_write == -1) {
                msg_resp_buffer_write = data[i];
//                this->ui->statusMessages->append(QString("Start of response size %1 bytes.").arg(msg_resp_buffer_write));
            } else {
//                qDebug() << QString("msg_resp_buffer.size()  %1, msg_resp_buffer_idx %2, data.size() %3, i %4").arg(msg_resp_buffer.size()).arg(msg_resp_buffer_idx).arg(data.size()).arg(i);
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
                        emit logMessage(QString("Error occurred during decode: %1.").arg(PB_GET_ERROR(&stream)));
                    } else {
                        if(msg_resp.which_payload == Response_ping_tag) {
                            emit pingResponse();
                        }
                        if(msg_resp.which_payload == Response_ack_tag) {
                            emit gotAck();
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
        emit logMessage(QString("Inject request too big.  %1.").arg(toInject.size()));
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

    writeRequest(r);
}
