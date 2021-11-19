#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QImage>
#include <QByteArray>
#include <QFile>
#include <QTextStream>
#include <QHostInfo>
#include <qMDNS.h>
#include <QStateMachine>
#include <QTimer>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QLabel>
#include "simple.pb.h"
#include "pb.h"
#include "rduworker.h"
#include <QThread>

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
    void foundHost();
    void pingResponse();
    void gotAck();
    void logCsv(bool log);
private slots:
    void workerStats(uint32_t packets, uint32_t badPackets);
    void workerFrame();


    void onDeviceDiscovered (const QHostInfo& info);
    void readyRead();
    void on_clk_gate_enable_clicked();

    void on_cpu_reset_clicked();

    void on_clk_gate_disable_clicked();

    void on_targetDynamic_clicked();

    void on_halt_cpu_clicked();

    void on_logCSV_stateChanged(int arg1);


protected:
    void closeEvent(QCloseEvent *event);
private:
    void setupStateMachine();
    void readCSRFile();
    void updateState(QString note);
    void connectPanelButton(QPushButton* but, MainWindow* target, QString onClick, QString onRelease);


    Ui::MainWindow *ui;

    QThread m_workerThread;
    RDUWorker m_worker;
    QByteArray m_framebuffer;

    QByteArray msg_resp_buffer;
    int msg_resp_buffer_write = -1;
    int msg_resp_buffer_idx = 0;

//    QMdnsEngine::Server server;
//    QMdnsEngine::Cache cache;
//    QMdnsEngine::Browser browser;
    QLabel* newWindow;
    qMDNS* m_mDNS;
    QStateMachine machine;
    QTimer mdnsQueryTimeout;
    QHostInfo rduHost;
    QTcpSocket socket;
    QTimer periodicPing;
    QTimer pingTimeout;
    QTimer setupTimeout;

    void writeWord(uint32_t addr, uint32_t data);
    void writeRequest(Request r);
    void writeInject(QByteArray toInject);
    void writeInjectHex(QString toInjectHex);

    uint32_t CLK_GATE = 0xf0003000;
    uint32_t CPU_RESET = 0xf0000800;

    struct {
        QByteArray press;
        QByteArray release;
    } typedef SimpleButton_t;
};
#endif // MAINWINDOW_H
