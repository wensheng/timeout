#include "timeoutservice.h"

TimeoutService::TimeoutService(int argc, char **argv)
    : QtService<QApplication>(argc, argv, "iehmonitor Service")
{
    setServiceDescription("iehmonitor Service");
    //this should not be paused
    //setServiceFlags(QtServiceBase::CanBeSuspended);

    /* No use: we use installer to tell windows to start service automatically
    QSettings bootUp("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run",
                         QSettings::NativeFormat);
    bootUp.setValue("/MyApp", "\"" + QDir::currentPath() + "/MyApp.exe\"" + " -a -u");
    */
}

TimeoutService::~TimeoutService(){
}

void TimeoutService::start(){
    //QCoreApplication *app = application();
    monitor = new TmMonitor;
}

void TimeoutService::stop(){
}

void TimeoutService::pause(){

}

void TimeoutService::resume(){

}

void TimeoutService::processCommand(int code){
    wchar_t ws[10];
    swprintf(ws, 10, L"%d", code);
    OutputDebugStringW(ws);
    if(code == 42){
        monitor->pauseTimer();
    }
}
