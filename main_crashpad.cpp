#define NOMINMAX
#include "mainwindow.h"
#include <QStandardPaths>

#if defined(Q_OS_MACOS)
    #include <mach-o/dyld.h>
#endif

#if defined(Q_OS_LINUX)
    #include <unistd.h>
    #define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif


#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <iostream>

#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"
#include "paths.h"
using namespace base;
using namespace crashpad;

bool initializeCrashpad(QString dbName, QString appName, QString appVersion, QString dataDir);
QString getExecutableDir(void);

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

    initializeCrashpad(dbName, appName, appVersion, appData);
    qInstallMessageHandler(myMessageOutput);

    MainWindow w;
    w.show();
    return a.exec();
}


bool initializeCrashpad(QString dbName, QString appName, QString appVersion, QString dataDir)
{
    // Get directory where the exe lives so we can pass a full path to handler, reportsDir and metricsDir
    QString exeDir = QCoreApplication::applicationDirPath();
    qInfo() << exeDir;
    // Helper class for cross-platform file systems
    Paths crashpadPaths(exeDir);

    // Ensure that crashpad_handler is shipped with your application
    FilePath handler(Paths::getPlatformString(crashpadPaths.getHandlerPath()));

    // Directory where reports will be saved. Important! Must be writable or crashpad_handler will crash.
    FilePath reportsDir(Paths::getPlatformString(dataDir));

    // Directory where metrics will be saved. Important! Must be writable or crashpad_handler will crash.
    FilePath metricsDir(Paths::getPlatformString(dataDir));

    // Configure url with your BugSplat database
    QString url = "https://" + dbName + ".bugsplat.com/post/bp/crash/crashpad.php";

    // Metadata that will be posted to BugSplat
    QMap<string, string> annotations;
    annotations["format"] = "minidump";                 // Required: Crashpad setting to save crash as a minidump
    annotations["database"] = dbName.toStdString();     // Required: BugSplat database
    annotations["product"] = appName.toStdString();     // Required: BugSplat appName
    annotations["version"] = appVersion.toStdString();  // Required: BugSplat appVersion
    annotations["key"] = "";                  // Optional: BugSplat key field
    annotations["user"] = "";          // Optional: BugSplat user email
    annotations["list_annotations"] = "";	// Optional: BugSplat crash description

    // Disable crashpad rate limiting so that all crashes have dmp files
    vector<string> arguments;
    arguments.push_back("--no-rate-limit");

    // Initialize crashpad database
    unique_ptr<CrashReportDatabase> database = CrashReportDatabase::Initialize(reportsDir);
    if (database == NULL) return false;

    // Enable automated crash uploads
    Settings *settings = database->GetSettings();
    if (settings == NULL) return false;
    settings->SetUploadsEnabled(true);

//    // Attachments to be uploaded alongside the crash - default bundle size limit is 20MB
//    vector<FilePath> attachments;

    // Start crash handler
    qInfo() << "Start crash handler. 123";
    CrashpadClient *client = new CrashpadClient();
    bool status = client->StartHandler(handler, reportsDir, metricsDir, url.toStdString(), annotations.toStdMap(), arguments, true, true);
//    bool status = client->StartHandler(handler, reportsDir, metricsDir, url.toStdString(), annotations.toStdMap(), arguments, true, true, attachments);
    qInfo() << QString("Crashpad start: %1").arg(status);
    return status;
}
