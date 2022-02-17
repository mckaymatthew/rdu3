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
#include <QRegularExpression>
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



    QList<QAction*> fpsActions = this->findChildren<QAction *>(QRegularExpression("actionFPS\\_(\\d+)"));
    for(auto act: fpsActions) {
        auto bound = std::bind(&MainWindow::action_FPS_triggered, this, act, std::placeholders::_1 );
        connect(act, &QAction::triggered, this, bound);
    }

    auto selectedFps = m_settings.value("mainWindow/frameRate",this->ui->actionFPS_30->objectName());
    auto toClick = this->findChild<QAction *>(selectedFps.toString());
    if(toClick != nullptr) {
        toClick->activate(QAction::ActionEvent::Trigger);
    }

    bool showConsole = m_settings.value("mainWindow/showConsole",QVariant::fromValue(true)).toBool();
    this->ui->actionShow_Console->setChecked(showConsole);
    this->on_actionShow_Console_toggled(showConsole);
//    this->ui->groupBox_3->setVisible(showConsole);
    restoreGeometry(m_settings.value("mainWindow/geometry").toByteArray());

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
//    qInfo() << QString("W: %1 H: %2").arg(w).arg(h);
    auto i = QPixmap::fromImage(fb);
    auto scaled = i.scaled(w,h,Qt::KeepAspectRatio);
//    qInfo() <<  QString("W: %1 H: %2").arg(scaled.width()).arg(scaled.height());
//    qInfo() << "";
    this->ui->renderZone->setPixmap(scaled);
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
//    this->ui->totalPackets->setText(QString("Packets: %1").arg(packets));
//    this->ui->totalPacketsBad->setText(QString("Bad Packets: %1").arg(badPackets));
//    this->ui->totalOOO->setText(QString("OOO Packets: %1").arg(oooPackets));
}


void MainWindow::workerFrame()
{
    m_framebuffer = m_worker->getCopy();
    QImage img((const uchar*) m_framebuffer.data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
    auto p = QPixmap::fromImage(img);
    int w = this->ui->renderZone->width();
    int h = this->ui->renderZone->height();
    auto s = p.scaled(w,h,Qt::KeepAspectRatio);
    this->ui->renderZone->setPixmap(s);

}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    drawError();
}
void MainWindow::closeEvent(QCloseEvent *event)
{

    QSettings settings("KE0PSL", "RDU3");
    qDebug() << "Close event" << settings.fileName();
    settings.setValue("mainWindow/geometry", saveGeometry());

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
    auto render = this->ui->renderZone->sizeHint();
    auto window = this->sizeHint();
    auto toHide = this->ui->statusMessages->maximumSize();
    qInfo() << QString("Render : W: %1 H: %2").arg(render.width()).arg(render.height());
    qInfo() << QString("Window : W: %1 H: %2").arg(window.width()).arg(window.height());
    qInfo() << QString("Console: W: %1 H: %2").arg(toHide.width()).arg(toHide.height());
    if(!arg1) {
    //    auto origMin = this->ui->renderZone->minimumSize();
        window.setHeight(window.height() - toHide.height());
//        window = render;
        qInfo() << QString("Smaller: W: %1 H: %2").arg(window.width()).arg(window.height());
        qInfo() << "";
    } else {
        window.setHeight(window.height() + toHide.height());
        qInfo() << QString("Bigger : W: %1 H: %2").arg(window.width()).arg(window.height());
        qInfo() << "";

    }
    this->ui->statusMessages->setVisible(arg1);
    this->resize(window);
    drawError();

    //    this->ui->centralwidget->adjustSize();
    //    this->adjustSize ();
}
void MainWindow::action_FPS_triggered(QAction* fps, bool) {
    bool ok = false;
    auto newFramerate = fps->objectName().right(2).toUInt(&ok);
    auto divisor = (60/newFramerate);
    updateState(QString("Framerate set to %1 FPS (divisor %2).").arg(newFramerate).arg(divisor));
    m_settings.setValue("mainWindow/frameRate",fps->objectName());
    m_controller.setFrameDivisor(divisor-1);
    QList<QAction*> fpsActions = this->findChildren<QAction *>(QRegularExpression("actionFPS\\_(\\d+)"));
    for(auto o : fpsActions) {
        if(o != fps) {
            o->setChecked(false);
        }
    }
}

