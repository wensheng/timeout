#include "timeoutservice.h"
#include "timeouteventfilter.h"
#include <QtGlobal>
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

TimeoutService::TimeoutService(int argc, char **argv)
    : QtService<QApplication>(argc, argv, "Time-Out Service")
{
    setServiceDescription("Time-Out Service");
    setServiceFlags(QtServiceBase::CanBeSuspended);
    wchar_t pszConsoleTitle[1024];
    wsprintf(pszConsoleTitle,L"%d/%d", GetTickCount(), GetCurrentProcessId());
    SetConsoleTitle(pszConsoleTitle);
    hwndFound=FindWindow(NULL, pszConsoleTitle);
    RegisterHotKey(hwndFound,   // Set the system identifier of the widget window that will handle the HotKey
                   100,                         // Set identifier HotKey
                   MOD_CONTROL | MOD_SHIFT,         // Set modifiers
                   'M');                        // HotKey
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
    qApp->setQuitOnLastWindowClosed(false);
    gui = new QLabel("t", 0, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    gui->move(QApplication::desktop()->availableGeometry().bottomRight());
    gui->show();
    //QCoreApplication *app = application();
    logfile.open("G:\\tmp\\logfile.txt", ofstream::app);
    qInstallMessageHandler(SimpleLoggingHandler);

    qDebug()<< "hello";
    TimeoutEventFilter *myfilter=new TimeoutEventFilter;
    //app->installNativeEventFilter(myfilter);
    QCoreApplication::instance()->installNativeEventFilter(myfilter);
    serviceMain = new ServiceMain();
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
