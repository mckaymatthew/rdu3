#include "mainwindow.h"
#include "rduwindow.h"
#include "RDUConstants.h"

#include <QApplication>

QAtomicInt g_NetworkBytesPerSecond;
QAtomicInt g_NetworkLinesPerSecond;
QAtomicInt g_NetworkFramesPerSecond;
QAtomicInt g_NetworkFramesTotal;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
