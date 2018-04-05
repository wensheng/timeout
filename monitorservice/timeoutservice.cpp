#include "timeoutservice.h"
#include "timeouteventfilter.h"
#include <QtGlobal>
/*
#include <fstream>

using namespace std;
ofstream logfile;
void SimpleLoggingHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    switch (type) {
    case QtInfoMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Info: " << msg.toLocal8Bit().constData() << "| in:" << context.file << context.line << context.function << "\n";
        break;
    case QtDebugMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Debug: " << msg.toLocal8Bit().constData() << "| in:" << context.file << " line:" << context.line << " func:" << context.function << "\n";
        break;
    case QtCriticalMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Critical: " << context.file << context.line << context.function << "\n";
        break;
    case QtWarningMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Warning: " << context.file << context.line << context.function << "\n";
        break;
    case QtFatalMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() <<  " Fatal: " << msg.toLocal8Bit().constData() << "\n";
        abort();
     }
    logfile.flush();
}
*/

TimeoutService::TimeoutService(int argc, char **argv)
    : QtService<QApplication>(argc, argv, "iehmonitor Service")
{
    /*
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString cLetter = env.value("SystemDrive");
    if(cLetter.isEmpty()){
        cLetter = "C:";
    }
    QString logPath = QString("%1\\ProgramData\\TimePie\\tm_monitor_service.log").arg(cLetter);
    wchar_t *logPath_w = new wchar_t[logPath.length() + 1];
    logfile.open(logPath_w, ofstream::app);
    qInstallMessageHandler(SimpleLoggingHandler);
    qDebug() << "tm monitor service log file is: " << logPath;
    */
    setServiceDescription("iehmonitor Service");
    //this should not be paused
    //setServiceFlags(QtServiceBase::CanBeSuspended);

    /*
    QSettings bootUp("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run",
                         QSettings::NativeFormat);
    bootUp.setValue("/MyApp", "\"" + QDir::currentPath() + "/MyApp.exe\"" + " -a -u");
    */
}

TimeoutService::~TimeoutService()
{
}

void TimeoutService::start()
{
    //QCoreApplication *app = application();
    qDebug()<< "hello";
    //TimeoutEventFilter *myfilter=new TimeoutEventFilter;
    //app->installNativeEventFilter(myfilter);
    //QCoreApplication::instance()->installNativeEventFilter(myfilter);
    monitor = new TmMonitor;
}

void TimeoutService::stop()
{
}

void TimeoutService::pause()
{

}

void TimeoutService::resume()
{

}

void TimeoutService::processCommand(int code)
{

}
