#include "mainwindow.h"
#include "RDUConstants.h"

#include <QApplication>
#include <QDir>
#include <QIODevice>
#include <QStandardPaths>
#include <QDateTime>
#include <iostream>
#include <stdio.h>
#include "client/linux/handler/exception_handler.h"

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
const char* uploaderPath = nullptr;
static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
void* context, bool succeeded) {
  fprintf(stderr, "Dump path: %s\n", descriptor.path());
  constexpr int cmdSize = 2048;
  char execmd[cmdSize];
  snprintf(execmd, cmdSize, "-p RDU3 -v 0.0.1 %s \"https://rdu3.bugsplat.com/post/bp/crash/postBP.php\"", descriptor.path());
  fprintf(stderr, "Dump exe: %s\n", uploaderPath);
  fprintf(stderr, "Dump arg: %s\n", execmd);

//  ./minidump_upload -p RDU3 -v 0.0.1 /tmp/8542cc4c-6f3c-4f6c-dc6713a1-42c10b95.dmp "https://rdu3.bugsplat.com/post/bp/crash/postBP.php"
  {
    pid_t pid = fork();
    if (pid == 0)
    {
      execl(uploaderPath,
            "minidump_upload",
            "-p","RDU3",
            "-v","0.0.1",
            descriptor.path(),
            "http://rdu3.bugsplat.com/post/bp/crash/postBP.php",
            (char *) 0);
      _exit(127);
    }
    else
      return true;
  }

  return succeeded;
}

int main(int argc, char *argv[])
{

    // Locate helper binary next to the current binary.
    char self_path[PATH_MAX];
    if (readlink("/proc/self/exe", self_path, sizeof(self_path) - 1) == -1) {
      exit(-101);
    }
    string helper_path(self_path);
    size_t pos = helper_path.rfind('/');
    if (pos == string::npos) {
      exit(-102);
    }
    helper_path.erase(pos + 1);
    helper_path += "minidump_upload";
    uploaderPath = helper_path.c_str();

    google_breakpad::MinidumpDescriptor descriptor("/tmp");
    google_breakpad::ExceptionHandler eh(descriptor, NULL, dumpCallback, NULL, true, -1);
    QString dbName = "rdu3";
    QString appName = "rdu3";
    QString appVersion = "0.0.1";


    QApplication a(argc, argv);
    QString appData = QStandardPaths::locate(QStandardPaths::GenericDataLocation,"",QStandardPaths::LocateDirectory) + a.applicationName() + "/";
    QDir appDataRdu = QDir(appData);
    if(!appDataRdu.exists()) {
        appDataRdu.mkpath(appData);
        qInfo() << "Making app path " << appData;
    }
    QString logPath = appData + "log.txt";
    qInfo() << "Logging to " << logPath;
    QFile logFile(logPath);
    logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);

    ts = new QTextStream(&logFile);

    qInstallMessageHandler(myMessageOutput);

    MainWindow w;
    w.show();
    return a.exec();
}
