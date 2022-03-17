#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QNetworkDatagram>
#include <QPixmap>
#include <QtEndian>
#include <QTextStream>
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
    , m_worker(new RDUWorker())
    , m_scaler(new scaler(m_worker))
    , m_controller()
{

    ui->setupUi(this);

    if(CSRMap::get().updated) {
        qInfo() << (QString("Updated CSR Mappings from file."));
        qInfo() << (QString("CLK GATE: 0x%1.").arg(CSRMap::get().CLK_GATE,8,16));
        qInfo() << (QString("CPU Reset: 0x%1.").arg(CSRMap::get().CPU_RESET,8,16));
    }
    QThread::currentThread()->setObjectName("GUI Thread");

    m_scaler->resized(this->ui->renderZone->size());
    m_workerThread = new QThread();
    auto scalerThread = new QThread();
    m_worker->moveToThread(m_workerThread);
    m_scaler->moveToThread(scalerThread);
    connect(&m_controller, &RDUController::notifyUserOfState, [this](QString msg){this->ui->stateLabel->setText(msg);});
    connect(&m_controller, &RDUController::RDUNotReady, [this](){ this->drawError();});
    connect(m_worker, &RDUWorker::message, [](QString msg){qInfo() << msg;});
//    connect(m_worker, &RDUWorker::newFrame, this, &MainWindow::workerFrame);
    connect(m_worker, &RDUWorker::newFrame, m_scaler, &scaler::newWork);
    connect(m_scaler,&scaler::newFrame, this, &MainWindow::scalerFrame);
    connect(this, &MainWindow::logCsv, m_worker, &RDUWorker::logPacketData);
    scalerThread->start();
    m_workerThread->start();
    QMetaObject::invokeMethod(m_worker,&RDUWorker::startWorker);
    QMetaObject::invokeMethod(m_scaler,&scaler::startWorker);
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

    auto zones = { this->ui->renderZone, this->ui->renderZone_2, this->ui->renderZone_3 };
    for(auto zone: zones) {
        connect(zone, &ClickableLabel::touch, this, &MainWindow::injectTouch);
        connect(zone, &ClickableLabel::release, this, &MainWindow::injectTouchRelease);
        connect(zone, &ClickableLabel::wheely, this, &MainWindow::tuneMainDial);
        connect(zone, &ClickableLabel::resized, m_scaler, &scaler::resized);
        zone->setAutoFillBackground(false);
        zone->setAttribute(Qt::WA_NoSystemBackground, true);
    }
    m_touchRearm.start(30);


    QString frameObj = m_settings.value("mainWindow/frameRate",QVariant("actionFPS_30")).toString();
    QAction* fpsAction = this->findChild<QAction *>(frameObj);
    if(fpsAction) {
        fpsAction->setChecked(true);
    }
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
    m_resizeTime.start();
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
void MainWindow::scalerFrame() {
    auto zone = whichLabel();

    auto workToDo = m_scaler->getCopy(m_framebufferImage);
    if(workToDo) {
        zone->toRender = m_framebufferImage;
        zone->update();
    }
}
void MainWindow::workerFrame()
{
    auto zone = whichLabel();
    int w = zone->width();
    int h = zone->height();

    auto workToDo = m_worker->getCopy(m_framebuffer);
    if(workToDo) {
        QImage img((const uchar*) m_framebuffer.data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
        m_resizeTime.restart();
        zone->toRender = img.scaled(w,h,Qt::KeepAspectRatio);
        m_resizeTimeLast =
                ((59.0* m_resizeTimeLast) +
                (m_resizeTime.nsecsElapsed() / 1000000)) / 60;
        g_ResizeTime = m_resizeTimeLast * g_scaleFactor;
        zone->update();
//        auto iNew = img.scaled(w,h,Qt::KeepAspectRatio);

//        if(m_lingering.isNull()) {
//            m_lingering = QPixmap::fromImage(iNew);
//        } else {
//            m_lingering.convertFromImage(iNew);
//        }
    //    qInfo() << m_lingering.cacheKey();
//        zone->setPixmap(m_lingering);
    }
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
    const QPixmap p = this->ui->renderZone->pixmap(Qt::ReturnByValue);
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
    qInfo() << (QString("Inject touch to %1,%2. Hex: %3 ").arg(l.x()).arg(l.y()).arg(QString(injectParameter.toHex())));
    m_controller.writeInject(injectParameter);

//    m_touchRearm.start(30);
}
void MainWindow::injectTouchRelease() {
    this->ui->lastActionLabel->setText(QString("Touch Release"));
    QByteArray injectParameter = QByteArray::fromHex("fe1300270f270ffd");
    qInfo() << (QString("Touch Release: %1.").arg(QString(injectParameter.toHex())));
    m_controller.writeInject(injectParameter);
}

void MainWindow::frontPanelButton_down(QString name, QByteArray d) {
    this->ui->lastActionLabel->setText(QString("Button Press: %1").arg(name));
    m_controller.writeInject(d);
    qInfo() << (QString("Inject press front panel button: %1, %2.").arg(name).arg(QString(d.toHex())));
    m_buttonDown.start();

}
void MainWindow::frontPanelButton_up(QString name, QByteArray d) {
    this->ui->lastActionLabel->setText(QString("Button Release: %1").arg(name));
    constexpr auto minTime = 50;
    auto downTime = m_buttonDown.elapsed();
    auto downLongEnough = downTime > minTime;
    if(downLongEnough) {
        m_controller.writeInject(d);
        qInfo() << (QString("Inject release front panel button: %1, %2.").arg(name).arg(QString(d.toHex())));
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
ClickableLabel* MainWindow::whichLabel() {
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


void MainWindow::on_actionGenerate_App_Crash_triggered()
{
    uint32_t* uhOh = nullptr;
    *uhOh = 0xFEEDBEEF;
}


void MainWindow::on_actionStats_for_Nerds_triggered()
{

    auto zones = { this->ui->renderZone, this->ui->renderZone_2, this->ui->renderZone_3 };
    for(auto zone: zones) {
        zone->stats = this->ui->actionStats_for_Nerds->isChecked();
    }
}

