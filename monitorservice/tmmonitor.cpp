#include "tmmonitor.h"

TmMonitor::TmMonitor(QObject *parent) : QObject(parent)
{
    qDebug() << "in TmMonitor()";
    QString timepiePath = "G:\\bitbucket\\timeout2\\build-timepie-Desktop_Qt_5_10_1_MSVC2015_32bit-Debug\\debug\\timepie.exe";
    timepieProgramPath = (const wchar_t*) timepiePath.utf16();
    /*
    auto restart = [&](int, QProcess::ExitStatus status = QProcess::CrashExit) {
        qDebug() << "got crash/exit status:"<<status;
        if (status == QProcess::CrashExit || status == QProcess::NormalExit)
          proc->start();
    };
    connect(proc, (void(QProcess::*)(int,QProcess::ExitStatus))&QProcess::finished, restart);
*/
    //restart(0);
    invokeTimepie();
}

void TmMonitor::invokeTimepie(){
    qDebug() << "trying to invoke timepie";
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    HANDLE hToken = NULL;
    /* WTSQueryUserToken only works when running as LocalSystem
     * when we run as service, service logon use LocalSystem account
     * so this can not used in interactive mode
     */
    bool queryResult = WTSQueryUserToken(sessionId, &hToken);
    if (!queryResult) {
        qDebug() << "can not query usertoken";
        DWORD err = GetLastError();
        qDebug() << err;
    }
    STARTUPINFO si = STARTUPINFO();
    si.cb = sizeof(STARTUPINFO);
    PROCESS_INFORMATION pi;

    //CreateProcessAsUserW( hToken, L"C:\\Windows\\notepad.exe", (LPWSTR)L"what", NULL, NULL,
    //                      FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi);
    CreateProcessAsUserW( hToken, timepieProgramPath, (LPWSTR)L"what", NULL, NULL,
                          FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi);
}
