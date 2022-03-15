#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QImage>
#include <QByteArray>
#include <QFile>
#include <QTextStream>
#include <QHostInfo>
#include <QTimer>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QLabel>
#include "simple.pb.h"
#include "pb.h"
#include "rduworker.h"
#include "rducontroller.h"
#include <QThread>
#include "csrmap.h"
#include <QSettings>
#include "clickablelabel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
signals:
    void logCsv(bool log);
public slots:
    void injectTouch(QPoint l);
    void injectTouchRelease();
private slots:
    void workerFrame();
    void tuneMainDial(int x);
    void on_actionInhibit_Transmit_triggered();
    void on_actionEnable_Transmit_triggered();
    void on_actionResetSOC_triggered();
    void on_actionHaltSOC_triggered();
    void on_actionLog_Network_Metadata_toggled(bool arg1);

    void action_FPS_triggered(QAction *, bool);
    void on_actionSave_PNG_triggered();
    void frontPanelButton_down(QString name, QByteArray d);
    void frontPanelButton_up(QString name, QByteArray d);

    ClickableLabel* whichLabel();

    void on_actionFull_triggered();

    void on_actionMinimal_triggered();

    void on_actionScreen_Only_triggered();

    void on_actionExit_FRONT_triggered();

    void on_actionGenerate_App_Crash_triggered();

protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void connectPanelButton(QPushButton* but, RDUController* target, QString onClick, QString onRelease);
    void drawError();

    Ui::MainWindow *ui;
    QSettings m_settings;
    QThread* m_workerThread;
    RDUWorker* m_worker;
    RDUController m_controller;
    QByteArray m_framebuffer;
    bool inhibit{false};
    QTimer m_touchRearm;
    QElapsedTimer m_buttonDown;
    QPixmap m_lingering;
};
#endif // MAINWINDOW_H
