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
//#include <qmdnsengine/server.h>
//#include <qmdnsengine/resolver.h>
//#include <qmdnsengine/cache.h>
//#include <qmdnsengine/browser.h>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

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
private slots:
    void processPendingDatagrams();
    void on_clearFb_clicked();
    void on_toPng_clicked();
    void on_pause_clicked();

    void onDeviceDiscovered (const QHostInfo& info);
    void readyRead();
    void on_clk_gate_enable_clicked();

    void on_cpu_reset_clicked();

    void on_clk_gate_disable_clicked();

    void on_targetDynamic_clicked();

    void on_halt_cpu_clicked();

protected:
    void closeEvent(QCloseEvent *event);
private:
    void updateState(QString note);

    Ui::MainWindow *ui;
    QUdpSocket m_incoming;
    QImage m_fb;
    QFile* m_logFile;
    QTextStream* m_stream = nullptr;
    QByteArray msg_resp_buffer;
    int msg_resp_buffer_write = -1;
    int msg_resp_buffer_idx = 0;
    int pkt_cnt = 0;
    int pkt_even_cnt = 0;
    int pkt_odd_cnt = 0;

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
    QElapsedTimer framesStart;

    void writeWord(uint32_t addr, uint32_t data);
    void writeRequest(Request r);


    uint32_t CLK_GATE = 0xf0003000;
    uint32_t CPU_RESET = 0xf0000800;
};
#endif // MAINWINDOW_H
