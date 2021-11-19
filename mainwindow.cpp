#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QNetworkDatagram>
#include <QPixmap>
#include <QtEndian>
#include <QTextStream>
#include <QSignalTransition>
#include "qmdnsengine/service.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include <QSettings>
#include <QPainter>
#include <QNetworkInterface>
#include "RDUConstants.h"

using namespace Qt;
using namespace std;

void MainWindow::connectPanelButton(QPushButton* but, MainWindow* target, QString onClick, QString onRelease) {
    but->connect(but, &QPushButton::pressed, std::bind(&MainWindow::writeInjectHex, target, onClick));
    but->connect(but, &QPushButton::released, std::bind(&MainWindow::writeInjectHex, target, onRelease));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
//    , server()
//    , cache()
//    , browser(&server, "_zephyr._tcp.local.", &cache)
    , m_mDNS(qMDNS::getInstance())
    , msg_resp_buffer(' ',Response_size)
    , msg_resp_buffer_write(-1)
    , m_worker()
{

    ui->setupUi(this);

    readCSRFile();
    setupStateMachine();

    connect(&socket, &QTcpSocket::readyRead,this, &MainWindow::readyRead);
    connect(&m_worker, &RDUWorker::message, [&](QString msg){this->ui->statusMessages->append(msg);});
    connect(&m_worker, &RDUWorker::newStats, this, &MainWindow::workerStats);
    connect(&m_worker, &RDUWorker::newFrame, this, &MainWindow::workerFrame);
    connect(this, &MainWindow::logCsv, &m_worker, &RDUWorker::logPacketData);

    m_worker.moveToThread(&m_workerThread);
    m_workerThread.start();
    QMetaObject::invokeMethod(&m_worker,&RDUWorker::startWorker);

    QString message = QString("No Signal");
    QImage fb(COLUMNS,LINES,QImage::Format_RGB16);
    fb.fill(QColor("Black"));
    QPainter p;
    p.begin(&fb);

    p.setPen(QPen(Qt::red));
    p.setFont(QFont("Courier New", 48, QFont::Bold));
    p.drawText(fb.rect(), Qt::AlignCenter, message);
    p.end();
    newWindow = new QLabel(nullptr);
    newWindow->setPixmap(QPixmap::fromImage(fb));
    newWindow->show();
    newWindow->raise();
    newWindow->setMinimumSize(COLUMNS, LINES);

    connectPanelButton(this->ui->buttonExit,this,"FE0E10FD","FE0E00FD");
    connectPanelButton(this->ui->button_Menu,this,"FE0D08FD","FE0D00FD");

    QSettings settings("KE0PSL", "RDU3");
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    newWindow->restoreGeometry(settings.value("newWindow/geometry").toByteArray());


    machine.start();
}
MainWindow::~MainWindow()
{
    writeWord(CLK_GATE,0);

    m_workerThread.quit();
    m_workerThread.wait();
    delete ui;
}

void MainWindow::workerStats(uint32_t packets, uint32_t badPackets)
{
    this->ui->totalPackets->setText(QString("Packets: %1").arg(packets));
    this->ui->totalPacketsBad->setText(QString("Bad Packets: %1").arg(badPackets));
}


void MainWindow::workerFrame()
{
    m_framebuffer = m_worker.getCopy();
    QImage img((const uchar*) m_framebuffer.data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
    auto p = QPixmap::fromImage(img);
    int w = newWindow->width();
    int h = newWindow->height();
    newWindow->setPixmap(p.scaled(w,h,Qt::KeepAspectRatio));

}

void MainWindow::closeEvent(QCloseEvent *event)
{

    QSettings settings("KE0PSL", "RDU3");
    qDebug() << "Close event" << settings.fileName();
//    settings.setValue("mainWindow/geometry", saveGeometry());
//    settings.setValue("newWindow/geometry", newWindow->saveGeometry());

    writeWord(CLK_GATE,0);
    QMainWindow::closeEvent(event);
}

void MainWindow::setupStateMachine()
{

    QState *errorRestart = new QState();
    QState *queryMDNS = new QState();
    QState *connectToRdu = new QState();
    QState *connected = new QState();
    QState *disablePixelClock = new QState();
    QState *setupHostData = new QState();
    QState *enablePixelClock = new QState();
    QState *ping = new QState();
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
    queryMDNS->addTransition(this,&MainWindow::foundHost, connectToRdu);
    auto connectedTransition = connectToRdu->addTransition(&socket,&QTcpSocket::connected, disablePixelClock);
    disablePixelClock->addTransition(this,&MainWindow::gotAck,setupHostData);
    setupHostData->addTransition(this,&MainWindow::gotAck,enablePixelClock);
    enablePixelClock->addTransition(this,&MainWindow::gotAck,connected);
    connected->addTransition(&periodicPing,&QTimer::timeout, ping);
    ping->addTransition(this,&MainWindow::pingResponse, connected);
\
    auto setupStates = {enablePixelClock, setupHostData, disablePixelClock};
    for(auto state: setupStates) {
        connect(state->addTransition(&setupTimeout, &QTimer::timeout,errorRestart), &QSignalTransition::triggered, [&]() {
            updateState("Time out during RDU setup.");
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
        updateState("Error Restart");
        this->ui->statusMessages->append(QString("Error occured. Socket state: %1.").arg(socket.errorString()));
        if(socket.isOpen()) {
            socket.close();
        }
    });

    connect(queryMDNS, &QState::entered, [this](){
        updateState("Query network for IC-7300 RDU");
        m_mDNS->lookup("rdu_ic7300.local");
        this->mdnsQueryTimeout.setSingleShot(true);
        this->mdnsQueryTimeout.start(5000);
    });
    connect(dnsTimeoutTransition,&QSignalTransition::triggered, [this](){
        updateState("Lookup time out");
    });
    connect(connectToRdu, &QState::entered, [this](){
//        auto addr = rduHost.addresses().first();
        QHostAddress addr = QHostAddress("10.0.0.128");
        updateState("Connect to IC-7300 RDU...");
        socket.connectToHost(addr, 4242);
    });
    connect(connectedTransition, &QSignalTransition::triggered, [this](){
        updateState("Connected.");
//        periodicPing.start(1000);
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
        updateState("Lost connection or device unresponsive.");
    });

    connect(disablePixelClock, &QState::entered, [this](){
        updateState("LCD Clock Inhibit.");
        writeWord(CLK_GATE,0);
        setupTimeout.setSingleShot(true);
        setupTimeout.start(1000);
    });
    connect(setupHostData, &QState::entered, [this](){
        updateState("Setup RDU FPGA Buffers.");
        Request r = Request_init_default;
        r.which_payload = Request_setupSlots_tag;
        writeRequest(r);
        setupTimeout.setSingleShot(true);
        setupTimeout.start(1000);
    });
    connect(enablePixelClock, &QState::entered, [this](){
        updateState("LCD Clock Enable.");
        writeWord(CLK_GATE,1);
        setupTimeout.setSingleShot(true);
        setupTimeout.start(1000);
    });

}

void MainWindow::readCSRFile()
{

    QFile csv("csr.csv");
    auto openResult = csv.open(QFile::ReadOnly | QFile::Text);
    if(!openResult) {

        this->ui->statusMessages->append(QString("Could not open CSR file.").arg(socket.errorString()));
    } else {
        bool CLK_GATE_OK = false;
        bool CPU_RESET_OK = false;
        QTextStream in(&csv);
        while (!in.atEnd()){
          QString s=in.readLine(); // reads line from file
          if(s.contains("rgb_control_csr")) {
              QStringList details = s.split(',');
              QString hexAddr = details[2].last(8);
              int value = hexAddr.toUInt(&CLK_GATE_OK, 16);
              if(CLK_GATE_OK) {
                  if (value == 0) {
                      CLK_GATE_OK = false;
                  } else {
                      this->CLK_GATE = value;
                      this->ui->statusMessages->append(QString("CLK GATE: 0x%1.").arg(this->CLK_GATE,8,16));
                  }
              }
          }
          if(s.contains("ctrl_reset")) {
              QStringList details = s.split(',');
              QString hexAddr = details[2].last(8);
              int value = hexAddr.toUInt(&CPU_RESET_OK, 16);
              if(CPU_RESET_OK) {
                  if (value == 0) {
                      CPU_RESET_OK = false;
                  } else {
                      this->CPU_RESET = value;
                      this->ui->statusMessages->append(QString("CPU Reset: 0x%1.").arg(this->CPU_RESET,8,16));
                  }
              }
          }
        }

    }
}
void MainWindow::updateState(QString note) {
    //Can get called during destruction
    if(this->ui && this->ui->statusMessages) {
        this->ui->statusMessages->append(note);
    }
}


void MainWindow::onDeviceDiscovered (const QHostInfo& info) {
//    this->ui->statusMessages->append(QString("Found device: %1 (%2)").arg(info.hostName()).arg(info.addresses().first().toString()));
    if(info.hostName().contains("rdu_ic7300.local")) {
        this->rduHost = info;
        emit foundHost();
        this->ui->statusMessages->append(QString("Found device: %1 (%2)").arg(info.hostName()).arg(info.addresses().first().toString()));
    }
}
void MainWindow::readyRead() {
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
                        this->ui->statusMessages->append(QString("Error occurred during decode: %1.").arg(PB_GET_ERROR(&stream)));
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
void MainWindow::writeInjectHex(QString toInjectHex) {
    writeInject(QByteArray::fromHex(toInjectHex.toLatin1()));
}
void MainWindow::writeInject(QByteArray toInject) {
    Request r = Request_init_default;
    if(toInject.size() > sizeof(r.payload.inject.data.bytes)) {
        this->ui->statusMessages->append(QString("Inject request too big.  %1.").arg(toInject.size()));
        return;
    }
    r.which_payload = Request_inject_tag;
    r.payload.inject.data.size = toInject.size();
    for(int i = 0; i < toInject.size(); i++) {
        r.payload.inject.data.bytes[i] = toInject[i];
    }
    writeRequest(r);

}
void MainWindow::writeRequest(Request r) {
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
void MainWindow::writeWord(uint32_t addr, uint32_t data) {

    Request r = Request_init_default;
    r.which_payload = Request_writeWord_tag;
    r.payload.writeWord.address = addr;
    r.payload.writeWord.data = data;

    writeRequest(r);
}
void MainWindow::on_clk_gate_enable_clicked()
{
    writeWord(CLK_GATE,1);
}

void MainWindow::on_clk_gate_disable_clicked()
{

    writeWord(CLK_GATE,0);
}

void MainWindow::on_cpu_reset_clicked()
{
    writeWord(CPU_RESET,1);
}

void MainWindow::on_targetDynamic_clicked()
{

    Request r = Request_init_default;
    r.which_payload = Request_setupSlots_tag;
    writeRequest(r);
}


void MainWindow::on_halt_cpu_clicked()
{
    writeWord(CPU_RESET+1,9); //unaligned write causes CPU to fault
}


void MainWindow::on_logCSV_stateChanged(int)
{
    emit logCsv(this->ui->logCSV->isChecked());
}
