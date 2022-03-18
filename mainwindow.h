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
#include <QSettings>
#include "renderlabel.h"
#include "ltxddecoder.h"
#include "lrxddecoder.h"
#include <QList>

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
    void buffDispose(QByteArray* d);
private slots:
    void workerFramePassthrough(QByteArray* f);
    void updateAction(QString action);
    void updateState(QString state);

    void action_FPS_triggered(QAction *, bool);
    void on_actionSave_PNG_triggered();
    void frontPanelButton_down(QString name, QByteArray d);
    void frontPanelButton_up(QString name, QByteArray d);

    RenderLabel* whichLabel();
    void changedViewport(int index, QWidget* active, QList<QWidget*> inactive);

    void on_actionGenerate_App_Crash_triggered();

protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void connectPanelButton(QPushButton* but, RDUController* target, QString onClick, QString onRelease);

    Ui::MainWindow *ui;
    QSettings m_settings;
    QThread* m_workerThread;
    RDUWorker* m_worker;
    RDUController m_controller;
    LtxdDecoder m_ltxd_decoder;
    LrxdDecoder m_lrxd_decoder;
    QElapsedTimer m_buttonDown;
    QList<RenderLabel*> m_zones;
    QList<QAction*> m_fpsActions;
};
#endif // MAINWINDOW_H
