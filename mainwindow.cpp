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

void MainWindow::connectPanelButton(QPushButton* but, RDUController* target, QString onClick, QString onRelease) {
    but->connect(but, &QPushButton::pressed, std::bind(&RDUController::writeInjectHex, target, onClick));
    but->connect(but, &QPushButton::released, std::bind(&RDUController::writeInjectHex, target, onRelease));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_worker()
    , m_controller()
{

    ui->setupUi(this);

    if(CSRMap::get().updated) {
        this->ui->statusMessages->append(QString("Updated CSR Mappings from file."));
        this->ui->statusMessages->append(QString("CLK GATE: 0x%1.").arg(CSRMap::get().CLK_GATE,8,16));
        this->ui->statusMessages->append(QString("CPU Reset: 0x%1.").arg(CSRMap::get().CPU_RESET,8,16));

    }

    connect(&m_controller, &RDUController::logMessage, [&](QString msg){this->ui->statusMessages->append(msg);});
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

    connectPanelButton(this->ui->buttonExit,&m_controller,"FE0E10FD","FE0E00FD");
    connectPanelButton(this->ui->button_Menu,&m_controller,"FE0D08FD","FE0D00FD");

    QSettings settings("KE0PSL", "RDU3");
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    newWindow->restoreGeometry(settings.value("newWindow/geometry").toByteArray());


}
MainWindow::~MainWindow()
{
    m_controller.writeWord(CSRMap::get().CLK_GATE,0);

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

    m_controller.writeWord(CSRMap::get().CLK_GATE,0);
    QMainWindow::closeEvent(event);
}

void MainWindow::updateState(QString note) {
    //Can get called during destruction
    if(this->ui && this->ui->statusMessages) {
        this->ui->statusMessages->append(note);
    }
}


void MainWindow::on_clk_gate_enable_clicked()
{
    m_controller.writeWord(CSRMap::get().CLK_GATE,1);
}

void MainWindow::on_clk_gate_disable_clicked()
{

    m_controller.writeWord(CSRMap::get().CLK_GATE,0);
}

void MainWindow::on_cpu_reset_clicked()
{
    m_controller.writeWord(CSRMap::get().CPU_RESET,1);
}


void MainWindow::on_halt_cpu_clicked()
{
    m_controller.writeWord(CSRMap::get().CPU_RESET+1,9); //unaligned write causes CPU to fault
}


void MainWindow::on_logCSV_stateChanged(int)
{
    emit logCsv(this->ui->logCSV->isChecked());
}
