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
#include <QDateTime>
#include <QtEndian>

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
        qInfo() << (QString("Updated CSR Mappings from file."));
        qInfo() << (QString("CLK GATE: 0x%1.").arg(CSRMap::get().CLK_GATE,8,16));
        qInfo() << (QString("CPU Reset: 0x%1.").arg(CSRMap::get().CPU_RESET,8,16));
    }
    QThread::currentThread()->setObjectName("GUI Thread");

    m_workerThread = new QThread();
    m_worker = new RDUWorker();
    m_worker->moveToThread(m_workerThread);
    connect(&m_controller, &RDUController::notifyUserOfState, [this](QString msg){this->ui->stateLabel->setText(msg);});
    connect(&m_controller, &RDUController::RDUNotReady, [this](){ this->drawError();});
    connect(m_worker, &RDUWorker::message, [](QString msg){qInfo() << msg;});
    connect(m_worker, &RDUWorker::newStats, this, &MainWindow::workerStats);
    connect(m_worker, &RDUWorker::newFrame, this, &MainWindow::workerFrame);
    connect(this, &MainWindow::logCsv, m_worker, &RDUWorker::logPacketData);

    m_workerThread->start();
    QMetaObject::invokeMethod(m_worker,&RDUWorker::startWorker);
    drawError();



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

    connect(this->ui->renderZone, &ClickableLabel::touch, this, &MainWindow::injectTouch);
    connect(this->ui->renderZone, &ClickableLabel::release, this, &MainWindow::injectTouchRelease);
    connect(this->ui->renderZone, &ClickableLabel::wheely, this, &MainWindow::tuneMainDial);
    connect(this->ui->renderZone_2, &ClickableLabel::touch, this, &MainWindow::injectTouch);
    connect(this->ui->renderZone_2, &ClickableLabel::release, this, &MainWindow::injectTouchRelease);
    connect(this->ui->renderZone_2, &ClickableLabel::wheely, this, &MainWindow::tuneMainDial);
    connect(this->ui->renderZone_3, &ClickableLabel::touch, this, &MainWindow::injectTouch);
    connect(this->ui->renderZone_3, &ClickableLabel::release, this, &MainWindow::injectTouchRelease);
    connect(this->ui->renderZone_3, &ClickableLabel::wheely, this, &MainWindow::tuneMainDial);
    m_touchRearm.start(30);


    QString frameObj = m_settings.value("mainWindow/frameRate",QVariant("actionFPS_30")).toString();
    QAction* fpsAction = this->findChild<QAction *>(frameObj);
    if(fpsAction) {
        fpsAction->setChecked(true);
    }
    m_controller.startController();


    QList<QPushButton*> frontPanelButtons = this->findChildren<QPushButton *>(QRegularExpression("fpb.+"));
    for(auto b : frontPanelButtons) {
        auto name = b->objectName();
        auto parts = name.split("_");
        if(parts.size() != 3 || parts[2].size() != 4) {
            qInfo() << QString("Button '%1' is setup wrong").arg(name);
        } else {
            auto textLabel = parts[1];
            auto enableStr = QString("fe%1fd").arg(parts[2]);
            auto disableStr = QString("fe%1fd").arg(parts[2].replace(2,2,"00"));
            auto bound_down = std::bind(&MainWindow::frontPanelButton_down, this, textLabel, QByteArray::fromHex(enableStr.toLatin1()));
            auto bound_up = std::bind(&MainWindow::frontPanelButton_up, this, textLabel, QByteArray::fromHex(disableStr.toLatin1()));
            connect(b, &QPushButton::pressed, bound_down);
            connect(b, &QPushButton::released, bound_up);
            qInfo() <<  QString("Button: %1, %2, %3, %4").arg(name).arg(textLabel).arg(enableStr).arg(disableStr);
        }
    }
//    QVariant index = m_settings.value("mainWindow/stackIndex", QVariant(0));
//    this->ui->stackedWidget->setCurrentIndex(index.toInt());
//    restoreGeometry(m_settings.value("mainWindow/geometry").toByteArray());
}

void MainWindow::drawError(){
    QString message = QString("No Signal");
    QImage fb(COLUMNS,LINES,QImage::Format_RGB16);
    fb.fill(QColor("Black"));
    QPainter p;
    p.begin(&fb);

    p.setPen(QPen(Qt::red));
    p.setFont(QFont("Courier New", 48, QFont::Bold));
    p.drawText(fb.rect(), Qt::AlignCenter, QString("%1").arg(message));
    p.end();
    auto zone = whichLabel();
    int w = zone->width();
    int h = zone->height();
//    qInfo() << QString("W: %1 H: %2").arg(w).arg(h);
    auto i = QPixmap::fromImage(fb);
    auto scaled = i.scaled(w,h,Qt::KeepAspectRatio, Qt::SmoothTransformation);
//    qInfo() <<  QString("W: %1 H: %2").arg(scaled.width()).arg(scaled.height());
//    qInfo() << "";
    zone->setPixmap(scaled);
    zone->setMinimumSize(COLUMNS, LINES);
}
MainWindow::~MainWindow()
{
    m_controller.writeWord(CSRMap::get().CLK_GATE,0);

    m_workerThread->quit();
    m_workerThread->wait();
    delete ui;
    ui = nullptr;
}

void MainWindow::workerStats(uint32_t packets, uint32_t badPackets, uint32_t oooPackets)
{
//    this->ui->totalPackets->setText(QString("Packets: %1").arg(packets));
//    this->ui->totalPacketsBad->setText(QString("Bad Packets: %1").arg(badPackets));
//    this->ui->totalOOO->setText(QString("OOO Packets: %1").arg(oooPackets));
}


void MainWindow::workerFrame()
{
    auto zone = whichLabel();
    m_framebuffer = m_worker->getCopy();
    QImage img((const uchar*) m_framebuffer.data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
    auto p = QPixmap::fromImage(img);
    int w = zone->width();
    int h = zone->height();
    auto s = p.scaled(w,h,Qt::KeepAspectRatio);
    zone->setPixmap(s);

}

void MainWindow::closeEvent(QCloseEvent *event)
{

    QSettings settings("KE0PSL", "RDU3");
    qInfo() << "Close event, save parameters to " << settings.fileName();
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/stackIndex", this->ui->stackedWidget->currentIndex());

    m_controller.writeWord(CSRMap::get().CLK_GATE,0);
    QMainWindow::closeEvent(event);
}

void MainWindow::on_actionInhibit_Transmit_triggered()
{
    this->ui->lastActionLabel->setText(QString("Debug Clk Inhibit"));
    qInfo() << "Debug: Inhibit Tx.";
    m_controller.writeWord(CSRMap::get().CLK_GATE,0);
}

void MainWindow::on_actionEnable_Transmit_triggered()
{
    this->ui->lastActionLabel->setText(QString("Debug Clk Enable"));
    qInfo() << ("Debug: Enable Tx.");
    m_controller.writeWord(CSRMap::get().CLK_GATE,1);
}

void MainWindow::on_actionResetSOC_triggered()
{
    this->ui->lastActionLabel->setText(QString("Rest CPU"));
    qInfo() << ("Debug: Rest SOC.");
    m_controller.writeWord(CSRMap::get().CPU_RESET,1);
}

void MainWindow::on_actionHaltSOC_triggered()
{
    this->ui->lastActionLabel->setText(QString("Halt CPU"));
    qInfo() << ("Debug: Halt SOC.");
    m_controller.writeWord(CSRMap::get().CPU_RESET+1,9); //unaligned write causes CPU to fault
}

void MainWindow::on_actionLog_Network_Metadata_toggled(bool arg1)
{
    qInfo() << (QString("Debug: High speed network metadata logging to %1.").arg(arg1?"Enabled":"Disabled"));
    emit logCsv(arg1);
}


void MainWindow::action_FPS_triggered(QAction* fps, bool) {
    bool ok = false;
    auto newFramerate = fps->objectName().right(2).toUInt(&ok);
    this->ui->lastActionLabel->setText(QString("Set FPS %1").arg(newFramerate));
    uint8_t divisor = (60/newFramerate);
    qInfo() << (QString("Framerate set to %1 FPS (divisor %2).").arg(newFramerate).arg(divisor));
    m_settings.setValue("mainWindow/frameRate",fps->objectName());
    m_controller.setFrameDivisor(divisor-1);
    QList<QAction*> fpsActions = this->findChildren<QAction *>(QRegularExpression("actionFPS\\_(\\d+)"));
    for(auto o : fpsActions) {
        if(o != fps) {
            o->setChecked(false);
        }
    }
}


void MainWindow::on_actionSave_PNG_triggered()
{
    auto p = this->ui->renderZone->pixmap();
    QDateTime time = QDateTime::currentDateTime();
    QString filename = QString("%1.png").arg(time.toString("dd.MM.yyyy.hh.mm.ss.z"));
    this->ui->lastActionLabel->setText(QString("Save PNG %1").arg(filename));
    QFile file(filename);
    file.open(QIODevice::WriteOnly);
    p.save(&file, "PNG");
    qInfo() << (QString("Saved frame to %1.").arg(filename));
}

void MainWindow::injectTouch(QPoint l) {
    if(l.x() < 0 || l.x() > 480) {
        return;
    }
    if(l.y() < 0 || l.y() > 272) {
        return;
    }
    if(m_touchRearm.remainingTime() != 0) {
//        return;
    }
    this->ui->lastActionLabel->setText(QString("Touch Point: %1,%2").arg(l.x()).arg(l.y()));
    quint16 x = qToBigEndian<quint16>(l.x());
    quint16 y = qToBigEndian<quint16>(l.y());
    QByteArray injectParameter = QByteArray::fromHex("fe1300");
    injectParameter.append(quint8(x>>0));
    injectParameter.append(quint8(x>>8));
    injectParameter.append(quint8(y>>0));
    injectParameter.append(quint8(y>>8));
    injectParameter.append(QByteArray::fromHex("fd"));
    qInfo() << (QString("Inject touch to %1,%2. Hex: %3 ").arg(l.x()).arg(l.y()).arg(injectParameter.toHex()));
    m_controller.writeInject(injectParameter);

//    m_touchRearm.start(30);
}
void MainWindow::injectTouchRelease() {
    this->ui->lastActionLabel->setText(QString("Touch Release"));
    QByteArray injectParameter = QByteArray::fromHex("fe1300270f270ffd");
    qInfo() << (QString("Touch Release: %1.").arg(injectParameter.toHex()));
    m_controller.writeInject(injectParameter);
}

void MainWindow::frontPanelButton_down(QString name, QByteArray d) {
    this->ui->lastActionLabel->setText(QString("Button Press: %1").arg(name));
    m_controller.writeInject(d);
    qInfo() << (QString("Inject press front panel button: %1, %2.").arg(name).arg(d.toHex()));
    m_buttonDown.start();

}
void MainWindow::frontPanelButton_up(QString name, QByteArray d) {
    this->ui->lastActionLabel->setText(QString("Button Release: %1").arg(name));
    constexpr auto minTime = 50;
    auto downTime = m_buttonDown.elapsed();
    auto downLongEnough = downTime > minTime;
    if(downLongEnough) {
        m_controller.writeInject(d);
        qInfo() << (QString("Inject release front panel button: %1, %2.").arg(name).arg(d.toHex()));
    } else {
        auto delayTime = minTime - downTime;
        qInfo() << QString("Defer up for %1 ms").arg(delayTime);
        auto bound_up = std::bind(&MainWindow::frontPanelButton_up, this, name, d);
        QTimer::singleShot(delayTime, bound_up);
    }
}

void MainWindow::tuneMainDial(int x) {
    this->ui->lastActionLabel->setText(QString("Dial %1").arg(x));
    qInfo() << (QString("Spin main dial, request %1.").arg(x));
    this->m_controller.spinMainDial(x);
}
QLabel* MainWindow::whichLabel() {
    if(this->ui->renderZone->isVisible()) {
        return this->ui->renderZone;
    }
    if(this->ui->renderZone_2->isVisible()) {
        return this->ui->renderZone_2;
    }
    if(this->ui->renderZone_3->isVisible()) {
        return this->ui->renderZone_3;
    }
    return this->ui->renderZone;
}

void MainWindow::on_actionFull_triggered()
{
    this->ui->stackedWidget->setCurrentIndex(0);
    this->ui->page_all->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->ui->page_minimal->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    this->ui->page_screen->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    QTimer::singleShot(5,[this](){
        this->resize(this->minimumSizeHint());
//        adjustSize();
        drawError();  //TODO: This is a hack to get widget to resize.
        //For some reason have to wait for event loop to cycle to get desired size?
    });
}


void MainWindow::on_actionMinimal_triggered()
{
    this->ui->stackedWidget->setCurrentIndex(1);
    this->ui->page_all->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    this->ui->page_minimal->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->ui->page_screen->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    QTimer::singleShot(5,[this](){
        this->resize(this->minimumSizeHint());
//        adjustSize();
        drawError(); //TODO: This is a hack to get widget to resize.
        //For some reason have to wait for event loop to cycle to get desired size?
    });
}


void MainWindow::on_actionScreen_Only_triggered()
{
    this->ui->stackedWidget->setCurrentIndex(2);
    this->ui->page_all->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    this->ui->page_minimal->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    this->ui->page_screen->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QTimer::singleShot(5,[this](){
        this->resize(this->minimumSizeHint());
//        adjustSize();
        drawError(); //TODO: This is a hack to get widget to resize.
        //For some reason have to wait for event loop to cycle to get desired size?
    });
}


void MainWindow::on_actionExit_FRONT_triggered()
{

}

