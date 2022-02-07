#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QImage>
#include <QByteArray>
#include <QFile>
#include <QTextStream>
#include <QHostInfo>
#include <QStateMachine>
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
private slots:
    void workerStats(uint32_t packets, uint32_t badPackets, uint32_t oooPackets);
    void workerFrame();

    void on_clk_gate_enable_clicked();
    void on_cpu_reset_clicked();
    void on_clk_gate_disable_clicked();
    void on_halt_cpu_clicked();
    void on_logCSV_stateChanged(int arg1);


protected:
    void closeEvent(QCloseEvent *event);
private:
    void updateState(QString note);
    void connectPanelButton(QPushButton* but, RDUController* target, QString onClick, QString onRelease);


    Ui::MainWindow *ui;

    QThread* m_workerThread;
    RDUWorker* m_worker;
    RDUController m_controller;
    QByteArray m_framebuffer;

    QLabel* newWindow;
};
#endif // MAINWINDOW_H
