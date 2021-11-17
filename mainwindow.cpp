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
namespace {


}
using namespace Qt;
using namespace std;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_fb(480,288,QImage::Format_RGB16)
    , m_logFile(nullptr)
//    , server()
//    , cache()
//    , browser(&server, "_zephyr._tcp.local.", &cache)
    , m_mDNS(qMDNS::getInstance())
    , msg_resp_buffer(' ',Response_size)
    , msg_resp_buffer_write(-1)
{

    framesStart.start();
    ui->setupUi(this);

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
//    QTimer::singleShot(1000,[this](){
//        auto p = QPixmap::fromImage(m_fb);

//        int w = newWindow->width();
//        int h = newWindow->height();
//        newWindow->setPixmap(p.scaled(w,h,Qt::KeepAspectRatio));
//    });
    connect(&socket, &QTcpSocket::readyRead,this, &MainWindow::readyRead);

    QString message = QString("No Signal");
    bool result =  m_incoming.bind(QHostAddress::AnyIPv4, 1337);
    if(!result) {
        message = QString("Failed to open port\nApp already open?");
        this->ui->statusMessages->append(QString("Open UDP Result: %1.").arg(result?"Success":"Fail"));
    }
    connect(&m_incoming, &QUdpSocket::readyRead, this, &MainWindow::processPendingDatagrams);


    m_fb.fill(QColor("Black"));
    QPainter p;
    p.begin(&m_fb);

    p.setPen(QPen(Qt::red));
//    p.setFont(QFont("Times", 48, QFont::Bold));
    p.setFont(QFont("Courier New", 48, QFont::Bold));

    p.drawText(m_fb.rect(), Qt::AlignCenter, message);
    p.end();
    newWindow = new QLabel(nullptr);
    newWindow->setPixmap(QPixmap::fromImage(m_fb));
    newWindow->show();
    newWindow->raise();
    newWindow->setMinimumSize(480, 288);
    connect (m_mDNS,  &qMDNS::hostFound,
             this,    &MainWindow::onDeviceDiscovered);


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


    if(openResult) {
        machine.start();
    }

    QSettings settings("KE0PSL", "RDU3");
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    newWindow->restoreGeometry(settings.value("newWindow/geometry").toByteArray());
}
MainWindow::~MainWindow()
{
    writeWord(CLK_GATE,0);
    delete ui;
}
void MainWindow::closeEvent(QCloseEvent *event)
{

    QSettings settings("KE0PSL", "RDU3");
    qDebug() << "Close event" << settings.fileName();
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("newWindow/geometry", newWindow->saveGeometry());

    writeWord(CLK_GATE,0);
    QMainWindow::closeEvent(event);
}
void MainWindow::updateState(QString note) {

    this->ui->statusMessages->append(note);
}

void MainWindow::processPendingDatagrams() {
    bool dispEven = this->ui->enEven->isChecked();
    bool dispOdd = this->ui->enOdd->isChecked();
    if(!m_logFile && this->ui->logCSV->isChecked()) {

        m_logFile = new QFile("log.csv",this);
        auto openRet = m_logFile->open(QIODevice::ReadWrite | QIODevice::Truncate);
        if(openRet) {
            this->ui->statusMessages->append("Open Log CSV.");
            m_stream = new QTextStream(m_logFile);
            *m_stream << "Packet,Time,LineNumber" << endl;
        } else {
        }
    }
    while (m_incoming.hasPendingDatagrams()) {
        pkt_cnt++;
        QNetworkDatagram datagram = m_incoming.receiveDatagram();
        auto pkt = datagram.data();
        auto header = pkt.data() + 2 ;
        uint32_t lineField = *((uint32_t*)header);
        if(lineField > 288) {
            continue;
        }

        if(m_stream) {
            auto time = framesStart.nsecsElapsed();
            *m_stream << QString("%1,%2,%3").arg(pkt_cnt).arg(time).arg(lineField) << endl;
        }
        uint16_t* asRGB = (uint16_t*)(pkt.data() + 6);

        bool doScanline = true;

        bool isEven = lineField % 2 == 0;
        pkt_even_cnt = pkt_even_cnt + (isEven?1:0);
        pkt_odd_cnt = pkt_odd_cnt + (isEven?0:1);
        if( (isEven && dispEven) || (!isEven && dispOdd)) {
            if(doScanline) {
                uchar* scanLine = m_fb.scanLine(lineField);
                memcpy(scanLine, asRGB, pkt.size() - 6);
            } else {
                for(int i = 0; i < 480; i++) {
                    auto dat = *(asRGB+i);
    //                m_fb.setPixelColor(i,lineField,dat);
                    uint16_t  red_mask = 0xF800;
                    uint16_t green_mask = 0x7E0;
                    uint16_t blue_mask = 0x1F;
                    int red_value = (dat & red_mask) >> 11;
                    int green_value = (dat & green_mask) >> 5;
                    int blue_value = (dat & blue_mask);

                    red_value  = red_value << 3;
                    green_value = green_value << 2;
                    blue_value  = blue_value << 3;
                    QColor rgbDat = QColor(red_value,green_value,blue_value);
                    m_fb.setPixelColor(i,lineField,rgbDat);
                }
            }
        }
    }
    auto p = QPixmap::fromImage(m_fb);

    int w = newWindow->width();
    int h = newWindow->height();
    newWindow->setPixmap(p.scaled(w,h,Qt::KeepAspectRatio));
//    if(this->ui->scale->isChecked()) {

//    int w = this->ui->fb_label->width();
//    int h = this->ui->fb_label->height();
//    if(this->ui->scale->isChecked()) {
//        this->ui->fb_label->setPixmap(p.scaled(480*this->ui->scaleBox->value(),288*this->ui->scaleBox->value(),Qt::KeepAspectRatio));
//    } else {
//        this->ui->fb_label->setPixmap(p);
//    }
//

    this->ui->evenCount->setText(QString("Even: %1").arg(pkt_even_cnt));
    this->ui->oddCount->setText(QString("Odd: %1").arg(pkt_odd_cnt));
    this->ui->deltaCount->setText(QString("Delta: %1").arg(pkt_even_cnt-pkt_odd_cnt));
}

void MainWindow::on_clearFb_clicked()
{
    m_fb.fill(QColor("Black"));

}

void MainWindow::on_toPng_clicked()
{
    QFile pngFile("fb.png");
    pngFile.open(QIODevice::WriteOnly);
    m_fb.save(&pngFile,"PNG");

}

void MainWindow::on_pause_clicked()
{
    this->ui->enEven->setChecked(!this->ui->enEven->isChecked());
    this->ui->enOdd->setChecked(!this->ui->enOdd->isChecked());
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

