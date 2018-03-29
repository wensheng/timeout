#include "timeoutservice.h"
#include "timeouteventfilter.h"


TimeoutService::TimeoutService(int argc, char **argv)
    : QtService<QApplication>(argc, argv, "Qt Interactive Service")
{
    setServiceDescription("A Qt service with user interface.");
    //setServiceFlags(QtServiceBase::CanBeSuspended);
    wchar_t pszConsoleTitle[1024];
    wsprintf(pszConsoleTitle,L"%d/%d", GetTickCount(), GetCurrentProcessId());
    SetConsoleTitle(pszConsoleTitle);
    hwndFound=FindWindow(NULL, pszConsoleTitle);
    RegisterHotKey(hwndFound,   // Set the system identifier of the widget window that will handle the HotKey
                   100,                         // Set identifier HotKey
                   MOD_ALT | MOD_SHIFT,         // Set modifiers
                   'Z');                        // HotKey
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

    QCoreApplication *app = application();
    qDebug()<< "hello";
    TimeoutEventFilter *myfilter=new TimeoutEventFilter;
    app->installNativeEventFilter(myfilter);
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
