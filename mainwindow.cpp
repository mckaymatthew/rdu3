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
#include <QDesktopServices>
#include <QDir>

using namespace Qt;
using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_settings("KE0PSL", "RDU3")
    , m_workerThread(nullptr)
    , m_worker(new RDUWorker())
    , m_controller()
{

    ui->setupUi(this);
    QThread::currentThread()->setObjectName("GUI Thread");

    m_workerThread = new QThread();
    m_worker->moveToThread(m_workerThread);
    connect(&m_controller, &RDUController::notifyUserOfState, this, &MainWindow::updateState);
    connect(&m_controller, &RDUController::notifyUserOfAction, this, &MainWindow::updateAction);
    connect(&m_controller, &RDUController::newLtxdBytes, &this->m_ltxd_decoder, &LtxdDecoder::newData);
    connect(&m_controller, &RDUController::newLrxdBytes, &this->m_lrxd_decoder, &LrxdDecoder::newData);
    connect(m_worker, &RDUWorker::newFrame, this, &MainWindow::workerFramePassthrough);
    connect(this, &MainWindow::buffDispose, m_worker, &RDUWorker::buffDispose);
    m_workerThread->start();
    QMetaObject::invokeMethod(m_worker,&RDUWorker::startWorker);

    //Connect up FPS actions to 'action FPS triggered'
    //Find saved selected FPS, click it (default to 30 fps)
    m_fpsActions = this->findChildren<QAction *>(QRegularExpression("actionFPS\\_(\\d+)"));
    auto selectedFps = m_settings.value("mainWindow/frameRate", this->ui->actionFPS_30->objectName());
    std::for_each(m_fpsActions.begin(), m_fpsActions.end(), [this, selectedFps](QAction *act ) {
        auto bound = std::bind(&MainWindow::action_FPS_triggered, this, act, std::placeholders::_1 );
        this->connect(act, &QAction::triggered, this, bound);
        if(act->objectName() == selectedFps) {
            act->activate(QAction::ActionEvent::Trigger);
        }
    });


    //Each view has it's own render zone
    //Connect up the touch/release, scroll wheel, stats selector.
    m_zones = this->findChildren<RenderLabel *>(QRegularExpression("renderZone\\_(\\d+)"));
    std::for_each(m_zones.begin(), m_zones.end(), [this](RenderLabel *zone ) {
        connect(zone, &RenderLabel::touch, &this->m_controller, &RDUController::injectTouch);
        connect(zone, &RenderLabel::release, &this->m_controller, &RDUController::injectTouchRelease);
        connect(zone, &RenderLabel::wheely, &this->m_controller, &RDUController::spinMainDial);
        connect(&m_controller, &RDUController::RDUReady, zone, &RenderLabel::drawError_n);
        connect(this->ui->actionStats_for_Nerds, &QAction::toggled, zone, &RenderLabel::showStats);
        zone->setAutoFillBackground(false);
        zone->setAttribute(Qt::WA_NoSystemBackground, true);

    });

    //Each front pannel button is named with the bytes that it needs to inject to be a "down"
    //press to the mainboard. Extract this, generate the corresponding "up" bytes, then
    //bind those to methods to handle it.
    //Note that buttons ARE duplicated, as each view has it's own set of buttons.
    auto frontPanelButtons = this->findChildren<QPushButton *>(QRegularExpression("fpb.+"));
    std::for_each(frontPanelButtons.begin(), frontPanelButtons.end(), [this](QPushButton *b ) {
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
//            qInfo() <<  QString("Button: %1, %2, %3, %4").arg(name).arg(textLabel).arg(enableStr).arg(disableStr);
        }
    });



    //There are menu items to select the view style, connect these up here.
    connect(this->ui->actionFull, &QAction::triggered,
            std::bind(&MainWindow::changedViewport, this, 0,
                 this->ui->page_all, QList<QWidget*>({this->ui->page_minimal, this->ui->page_screen})));
    connect(this->ui->actionMinimal, &QAction::triggered,
            std::bind(&MainWindow::changedViewport, this, 1,
                 this->ui->page_minimal, QList<QWidget*>({this->ui->page_all, this->ui->page_screen})));
    connect(this->ui->actionScreen_Only, &QAction::triggered,
            std::bind(&MainWindow::changedViewport, this, 2,
                 this->ui->page_screen, QList<QWidget*>({this->ui->page_all, this->ui->page_minimal})));


    //Connect action to make huge csv files..
    connect(this->ui->actionLog_Network_Metadata, &QAction::toggled, m_worker, &RDUWorker::logPacketData);

//    QVariant index = m_settings.value("mainWindow/stackIndex", QVariant(0));
//    this->ui->stackedWidget->setCurrentIndex(index.toInt());
//    restoreGeometry(m_settings.value("mainWindow/geometry").toByteArray());

    connect(this->ui->renderZone_1, &RenderLabel::wheeld, [this](QWheelEvent * e){

        QCoreApplication::postEvent(this->ui->mainDial, e->clone());
    });

    connect(this->ui->actionInhibit_Transmit, &QAction::triggered, std::bind(&RDUController::writeWord, &m_controller, CLK_GATE, 0));
    connect(this->ui->actionInhibit_Transmit, &QAction::triggered, std::bind(&MainWindow::updateAction, this, "Tx Inhibit"));

    connect(this->ui->actionEnable_Transmit, &QAction::triggered, std::bind(&RDUController::writeWord, &m_controller, CLK_GATE, 1));
    connect(this->ui->actionEnable_Transmit, &QAction::triggered, std::bind(&MainWindow::updateAction, this, "Tx Enable"));

    connect(this->ui->actionResetSOC, &QAction::triggered, std::bind(&RDUController::writeWord, &m_controller, CPU_RESET, 1));
    connect(this->ui->actionResetSOC, &QAction::triggered, std::bind(&MainWindow::updateAction, this, "Reset SoC"));

     //unaligned write causes CPU to fault
    connect(this->ui->actionHaltSOC, &QAction::triggered, std::bind(&RDUController::writeWord, &m_controller, CPU_RESET+1, 9));
    connect(this->ui->actionHaltSOC, &QAction::triggered, std::bind(&MainWindow::updateAction, this, "Halt SOC"));


    connect(this->ui->mainDial, &QDial::valueChanged, &this->m_mainDialAccumulator, &Accumulator::input);
    connect(this->ui->multiDial, &QDial::valueChanged, &this->m_multiDialAccumulator, &Accumulator::input);
    connect(this->ui->bpf_in, &QDial::valueChanged, &this->m_bpfInAccumulator, &Accumulator::input);
    connect(this->ui->bpf_out, &QDial::valueChanged, &this->m_bpfOutAccumulator, &Accumulator::input);

    connect(&m_mainDialAccumulator, &Accumulator::output, &m_controller, &RDUController::spinMainDial);
    connect(&m_multiDialAccumulator, &Accumulator::output, &m_controller, &RDUController::spinMultiDial);
    connect(&m_bpfInAccumulator, &Accumulator::output, &m_controller, &RDUController::spinBPFInDial);
    connect(&m_bpfOutAccumulator, &Accumulator::output, &m_controller, &RDUController::spinBPFOutDial);

    m_mainDialAccumulator.setMax(360);
    m_multiDialAccumulator.setMax(20);
    m_bpfInAccumulator.setMax(20);
    m_bpfOutAccumulator.setMax(20);

}


MainWindow::~MainWindow()
{
    m_controller.writeWord(CLK_GATE,0);

    m_workerThread->quit();
    m_workerThread->wait();
    delete ui;
    ui = nullptr;
}

void MainWindow::workerFramePassthrough(QByteArray* f) {
    auto zone = whichLabel();
    if(zone->imageBacking != nullptr) {
        //If there is a buffer there, return it to the UDP worker
        emit this->buffDispose(zone->imageBacking);
        zone->imageBacking = nullptr;
    }
    //Take the buffer from the UDP woker and store it
    zone->imageBacking = f;
    if(zone->imageBacking != nullptr) {
        //If we have a backing buffer, create a qimage.
        //Note that qimage passes through the buffer, no copy is made.
        zone->toRender = QImage((const uchar*) f->data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
        zone->repaint();
    }

}
void MainWindow::closeEvent(QCloseEvent *event)
{

    QSettings settings("KE0PSL", "RDU3");
    qInfo() << "Close event, save parameters to " << settings.fileName();
//    settings.setValue("mainWindow/geometry", saveGeometry());
//    settings.setValue("mainWindow/stackIndex", this->ui->stackedWidget->currentIndex());

    //Shut off FPGA Tx
    m_controller.writeWord(CLK_GATE,0);
    QMainWindow::closeEvent(event);
}



void MainWindow::updateAction(QString action) {
    qInfo() << "Action: " << action;
    this->ui->lastActionLabel->setText(action);
}
void MainWindow::updateState(QString state){
    if(state != m_stateLast) {
        qInfo() << "State: " <<  state;
    }
    m_stateLast = state;
    this->ui->stateLabel->setText(state);
}
void MainWindow::action_FPS_triggered(QAction* fps, bool) {
    bool ok = false;
    auto newFramerate = fps->objectName().right(2).toUInt(&ok);
    this->ui->lastActionLabel->setText(QString("Set FPS %1").arg(newFramerate));
    uint8_t divisor = (60/newFramerate);
    qInfo() << (QString("Framerate set to %1 FPS (divisor %2).").arg(newFramerate).arg(divisor));
    m_settings.setValue("mainWindow/frameRate",fps->objectName());
    m_controller.setFrameDivisor(divisor-1);

    std::for_each(m_fpsActions.begin(), m_fpsActions.end(), [fps](QAction *act ) {
        if(act != fps) {
            act->setChecked(false);
        }
    });

}


void MainWindow::on_actionSave_PNG_triggered()
{
    auto p = whichLabel()->toRender;
    QDateTime time = QDateTime::currentDateTime();
    QString filename = QString("%1.png").arg(time.toString("dd.MM.yyyy.hh.mm.ss.z"));
    this->ui->lastActionLabel->setText(QString("Save PNG %1").arg(filename));
    QFile file(filename);
    file.open(QIODevice::WriteOnly);
    p.save(&file, "PNG");
    qInfo() << (QString("Saved frame to %1.").arg(filename));
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

RenderLabel* MainWindow::whichLabel() {
    for(auto zone: m_zones) {
        if(zone->isVisible()) {
            return zone;
        }
    }
    return this->ui->renderZone_1;
}

void MainWindow::changedViewport(int index, QWidget* active, QList<QWidget*> inactive) {
    this->ui->stackedWidget->setCurrentIndex(index);
    active->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    std::for_each(inactive.begin(), inactive.end(), [](QWidget* off){
        off->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    });
    QTimer::singleShot(5,[this](){
        this->resize(this->minimumSizeHint());
//        adjustSize();
        //TODO: This is a hack to get widget to resize.
        //For some reason have to wait for event loop to cycle to get desired size?
    });
}
void MainWindow::on_actionGenerate_App_Crash_triggered()
{
    uint32_t* uhOh = nullptr;
    *uhOh = 0xFEEDBEEF;
}


void MainWindow::on_actionOpen_log_file_triggered()
{
    QString logPath = "file://" +QDir::tempPath() + "/RDU.txt";
    qInfo() << QString("Requesting OS to open %1").arg(logPath);
    QDesktopServices::openUrl(logPath);
}
