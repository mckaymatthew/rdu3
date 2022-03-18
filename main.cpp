#include "mainwindow.h"
#include "RDUConstants.h"

#include <QApplication>
#include <QDir>
#include <QIODevice>

#include <iostream>

QAtomicInt g_NetworkBytesPerSecond;
QAtomicInt g_NetworkLinesPerSecond;
QAtomicInt g_NetworkFramesPerSecond;
QAtomicInt g_NetworkFramesTotal;
QAtomicInt g_FramesLostNoBuffer;
QAtomicInt g_packetsTotal;
QAtomicInt g_packetsRejected;

QTextStream* ts;

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QDateTime now = QDateTime::currentDateTime();
    QString txt;
    switch (type) {
    case QtDebugMsg:
        txt = QString("%2 [DEBUG] %1").arg(msg).arg(now.toString("yyyy.MM.dd hh.mm.ss.zzz"));
        break;
    case QtInfoMsg:
        txt = QString("%2 [INFO ] %1").arg(msg).arg(now.toString("yyyy.MM.dd hh.mm.ss.zzz"));
        break;
    case QtWarningMsg:
        txt = QString("%2 [WARN ] %1").arg(msg).arg(now.toString("yyyy.MM.dd hh.mm.ss.zzz"));
    break;
    case QtCriticalMsg:
        txt = QString("%2 [CRITI] %1").arg(msg).arg(now.toString("yyyy.MM.dd hh.mm.ss.zzz"));
    break;
    case QtFatalMsg:
        txt = QString("%2 [FATAL] %1").arg(msg).arg(now.toString("yyyy.MM.dd hh.mm.ss.zzz"));
        abort();
    }

    std::cout << txt.toLatin1().data() << std::endl << std::flush;
    *ts << txt << Qt::endl;
    ts->flush();
}


int main(int argc, char *argv[])
{
    QString logPath = QDir::tempPath() + QDir::separator() + "RDU.txt";
    QFile logFile(logPath);
    logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    ts = new QTextStream(&logFile);

    qInstallMessageHandler(myMessageOutput);
    qInfo() << QString("Logging to %1").arg(logPath);
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
