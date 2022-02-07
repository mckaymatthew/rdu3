#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QNetworkDatagram>
#include <QPixmap>
#include <QtEndian>
#include <QTextStream>
#include <QSignalTransition>
#include "qmdnsengine/service.h"
#include <QSettings>
#include <QPainter>
#include <QNetworkInterface>
#include "RDUConstants.h"

using namespace Qt;
using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_settings("KE0PSL", "RDU3")
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_controller()
{

    ui->setupUi(this);

    if(CSRMap::get().updated) {
        this->ui->statusMessages->append(QString("Updated CSR Mappings from file."));
        this->ui->statusMessages->append(QString("CLK GATE: 0x%1.").arg(CSRMap::get().CLK_GATE,8,16));
        this->ui->statusMessages->append(QString("CPU Reset: 0x%1.").arg(CSRMap::get().CPU_RESET,8,16));
    }
    QThread::currentThread()->setObjectName("GUI Thread");

    m_workerThread = new QThread();
    m_worker = new RDUWorker(m_workerThread);
//    m_worker->moveToThread(m_workerThread);
    connect(&m_controller, &RDUController::logMessage, [&](QString msg){this->ui->statusMessages->append(msg);});
    connect(&m_controller, &RDUController::logMessageWithError, [&](QString msg, QString err){
        this->ui->statusMessages->append(msg);
        this->drawError(err);
    });

    connect(m_worker, &RDUWorker::message, [&](QString msg){this->ui->statusMessages->append(msg);});
    connect(m_worker, &RDUWorker::newStats, this, &MainWindow::workerStats);
    connect(m_worker, &RDUWorker::newFrame, this, &MainWindow::workerFrame);
    connect(this, &MainWindow::logCsv, m_worker, &RDUWorker::logPacketData);

    m_workerThread->start();
    QMetaObject::invokeMethod(m_worker,&RDUWorker::startWorker);
    drawError("");

    bool showConsole = m_settings.value("mainWindow/showConsole",QVariant::fromValue(true)).toBool();
    this->ui->actionShow_Console->setChecked(showConsole);
    this->on_actionShow_Console_toggled(showConsole);
//    this->ui->groupBox_3->setVisible(showConsole);
//    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());

}
void MainWindow::drawError() {
    drawError(m_errorLast);
}
void MainWindow::drawError(QString error){
    m_errorLast = error;
    QString message = QString("No Signal");
    QImage fb(COLUMNS,LINES,QImage::Format_RGB16);
    fb.fill(QColor("Black"));
    QPainter p;
    p.begin(&fb);

    p.setPen(QPen(Qt::red));
    p.setFont(QFont("Courier New", 48, QFont::Bold));
    p.drawText(fb.rect(), Qt::AlignCenter, QString("%1\n%2").arg(message).arg(error));
    p.end();
    int w = this->ui->renderZone->width();
    int h = this->ui->renderZone->height();
    this->ui->renderZone->setPixmap(QPixmap::fromImage(fb).scaled(w,h,Qt::KeepAspectRatio));
    this->ui->renderZone->setMinimumSize(COLUMNS, LINES);
}
MainWindow::~MainWindow()
{
    m_controller.writeWord(CSRMap::get().CLK_GATE,0);

    m_workerThread->quit();
    m_workerThread->wait();
    delete ui;
}

void MainWindow::workerStats(uint32_t packets, uint32_t badPackets, uint32_t oooPackets)
{
    this->ui->totalPackets->setText(QString("Packets: %1").arg(packets));
    this->ui->totalPacketsBad->setText(QString("Bad Packets: %1").arg(badPackets));
    this->ui->totalOOO->setText(QString("OOO Packets: %1").arg(oooPackets));
}


void MainWindow::workerFrame()
{
    m_framebuffer = m_worker->getCopy();
    QImage img((const uchar*) m_framebuffer.data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
    auto p = QPixmap::fromImage(img);
    int w = this->ui->renderZone->width();
    int h = this->ui->renderZone->height();
    this->ui->renderZone->setPixmap(p.scaled(w,h,Qt::KeepAspectRatio));

}

void MainWindow::resizeEvent(QResizeEvent *event) {
    drawError();
    QMainWindow::resizeEvent(event);
}
void MainWindow::closeEvent(QCloseEvent *event)
{

    QSettings settings("KE0PSL", "RDU3");
    qDebug() << "Close event" << settings.fileName();
//    settings.setValue("mainWindow/geometry", saveGeometry());
//    settings.setValue("newWindow/geometry", newWindow->saveGeometry());

    m_controller.writeWord(CSRMap::get().CLK_GATE,0);
    QMainWindow::closeEvent(event);
}

void MainWindow::updateState(QString note) {
    //Can get called during destruction
    if(this->ui && this->ui->statusMessages) {
        this->ui->statusMessages->append(note);
    }
}


void MainWindow::clickPanelButton(QString onClick, QString onRelease, int delayMs) {
    m_controller.writeInjectHex(onClick);
    QTimer::singleShot(delayMs, [this, onRelease]() {
        this->m_controller.writeInjectHex(onRelease);
    });
}

void MainWindow::on_actionExit_FRONT_triggered()
{
    updateState("Pressing Exit button.");
    clickPanelButton("FE0E10FD","FE0E00FD", 100);
}

void MainWindow::on_actionMenu_triggered()
{
    updateState("Pressing Menu button.");
    clickPanelButton("FE0D08FD","FE0D00FD", 100);
}

void MainWindow::on_actionInhibit_Transmit_triggered()
{
    updateState("Debug: Inhibit Tx.");
    m_controller.writeWord(CSRMap::get().CLK_GATE,0);
}

void MainWindow::on_actionEnable_Transmit_triggered()
{
    updateState("Debug: Enable Tx.");
    m_controller.writeWord(CSRMap::get().CLK_GATE,1);
}

void MainWindow::on_actionResetSOC_triggered()
{
    updateState("Debug: Rest SOC.");
    m_controller.writeWord(CSRMap::get().CPU_RESET,1);
}

void MainWindow::on_actionHaltSOC_triggered()
{
    updateState("Debug: Halt SOC.");
    m_controller.writeWord(CSRMap::get().CPU_RESET+1,9); //unaligned write causes CPU to fault
}

void MainWindow::on_actionLog_Network_Metadata_toggled(bool arg1)
{
    updateState(QString("Debug: High speed network metadata logging to %1.").arg(arg1?"Enabled":"Disabled"));
    emit logCsv(arg1);
}


void MainWindow::on_actionShow_Console_toggled(bool arg1)
{
    m_settings.setValue("mainWindow/showConsole", arg1);
    if(!arg1) {
        auto currentSize = size();
        auto removeSize = this->ui->groupBox_3->size();
        auto newHeight = currentSize.height() - removeSize.height();
        this->resize(currentSize.width(), newHeight);
    }
    this->ui->groupBox_3->setVisible(arg1);

}

