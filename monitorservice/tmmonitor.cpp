#include "tmmonitor.h"
#include <fstream>
//using namespace std;

std::ofstream logfile;
void SimpleLoggingHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    switch (type) {
    case QtInfoMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Info: " << msg.toLocal8Bit().constData() << "| in:" << context.file << context.line << context.function << "\n";
        break;
    case QtDebugMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Debug: " << msg.toLocal8Bit().constData() << "| in:" << context.file << " line:" << context.line << " func:" << context.function << "\n";
        break;
    case QtCriticalMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Critical: " << msg.toLocal8Bit().constData() << "| in:" << context.file << context.line << context.function << "\n";
        break;
    case QtWarningMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Warning: " << msg.toLocal8Bit().constData() << "| in:" << context.file << context.line << context.function << "\n";
        break;
    case QtFatalMsg:
        logfile << QTime::currentTime().toString().toLocal8Bit().constData() <<  " Fatal: " << msg.toLocal8Bit().constData() << "\n";
        abort();
     }
    logfile.flush();
}
TmMonitor::TmMonitor(QObject *parent) : QObject(parent)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString cLetter = env.value("SystemDrive");
    if(cLetter.isEmpty()){
        cLetter = "C:";
    }
    progDataDir = QString("%1\\ProgramData\\TimePie").arg(cLetter);
    QString logPath = QString("%1\\ProgramData\\TimePie\\tm_monitor_service.log").arg(cLetter);
    qDebug() << "tm monitor service log file is: " << logPath;
    logfile.open(logPath.toStdString(), std::ofstream::app);
    qInstallMessageHandler(SimpleLoggingHandler);

    qDebug() << "in TmMonitor()";
    QString timepiePath = "G:\\bitbucket\\timeout2\\build-timepie-Desktop_Qt_5_10_1_MSVC2015_32bit-Debug\\debug\\timepie.exe";
    wcscpy(timepieProgramPath, timepiePath.toStdWString().c_str());

    /* This works in interactive mode, but doesn't work in service
    auto restart = [&](int, QProcess::ExitStatus status = QProcess::CrashExit) {
        qDebug() << "got crash/exit status:"<<status;
        if (status == QProcess::CrashExit || status == QProcess::NormalExit)
          proc->start();
    };
    connect(proc, (void(QProcess::*)(int,QProcess::ExitStatus))&QProcess::finished, restart);
    restart(0);
   */
    oneMinuteTimer = new QTimer;
    connect(oneMinuteTimer, SIGNAL(timeout()), this, SLOT(restartTimePieIfNotRunning()));
    oneMinuteTimer->start(60000);
    restartTimePieIfNotRunning();

    //getActiveSessionUserName();
    //invokeTimepie();
}

bool TmMonitor::getActiveSessionUserName(){
    bool result;
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    HANDLE hToken = NULL;
    /* WTSQueryUserToken only works when running as LocalSystem
     * when we run as service, service logon use LocalSystem account
     * so this can not used in interactive mode
     */
    result = WTSQueryUserToken(sessionId, &hToken);
    if (!result) {
        qDebug() << "can not query usertoken";
        DWORD err = GetLastError();
        qDebug() << err;
        CloseHandle(hToken);
        return false;
    }

    DWORD tokenInfoLength = 0;
    //first call gets length of TokenInformation
    result = GetTokenInformation(hToken,
                                 TokenUser,
                                 NULL,
                                 0,
                                 &tokenInfoLength);
    if(!result && GetLastError() != ERROR_INSUFFICIENT_BUFFER){
        qDebug() << "could not get token info length, err:" << GetLastError();
        CloseHandle(hToken);
        return false;
    }

    std::vector<BYTE> buffer;
    buffer.resize(tokenInfoLength);
    PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(&buffer[0]);
    result = GetTokenInformation(hToken,
                                 TokenUser,
                                 pTokenUser,
                                 tokenInfoLength,
                                 &tokenInfoLength);
    if(!result){
        qDebug() << "could not get user tokeninfo, err:" << GetLastError();
        CloseHandle(hToken);
        return false;
    }
    qDebug() << "current session user sid:" << pTokenUser->User.Sid;
    if(!IsValidSid(pTokenUser->User.Sid)){
        qDebug() << "user sid not valid";
        CloseHandle(hToken);
        return false;
    }
    DWORD dwSize = MAX_NAME_SIZE;
    SID_NAME_USE sidType;
    wchar_t lpName[MAX_NAME_SIZE];
    wchar_t lpDomain[MAX_NAME_SIZE];
    result = LookupAccountSidW(NULL,
                               pTokenUser->User.Sid,
                               lpName,
                               &dwSize,
                               lpDomain,
                               &dwSize,
                               &sidType);
    if(!result){
        qDebug() << "could not look up sid";
        CloseHandle(hToken);
        return false;
    }
    activeUserName = QString::fromWCharArray(lpName);
    return true;
}

QString TmMonitor::getProcessName( DWORD processID ){
    TCHAR szProcessName[256] = TEXT("");
    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );

    // Get the process name.
    if (NULL != hProcess ){
        HMODULE hMod;
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)){
            GetModuleBaseName( hProcess, hMod, szProcessName,
                               sizeof(szProcessName)/sizeof(TCHAR) );
        }
    }

    CloseHandle( hProcess );
    return QString::fromWCharArray(szProcessName);
}

void TmMonitor::restartTimePieIfNotRunning(){
    if(!getActiveSessionUserName()){
        return;
    }
    qDebug() << "current user is " << activeUserName;
    QString pidFilePath = QString("%1\\%2.pid").arg(progDataDir).arg(QString::fromLatin1(activeUserName.toUtf8().toBase64()));
    qDebug() << "pidfile is: " << pidFilePath;
    QFile pidFile(pidFilePath);
    if(!pidFile.exists()){
        qDebug() << "pid doesn't exist";
        invokeTimepie();
    }else{
        pidFile.open(QIODevice::ReadOnly);
        DWORD tmpid = atol(pidFile.readAll());
        pidFile.close();
        qDebug() << "pid exist: " << tmpid;
        //when testing, since this is 32bit program
        // we can only get another 32bit program name
        QString tmProcessName = getProcessName(tmpid);
        if(tmProcessName.isEmpty()){
            invokeTimepie();
        }else{
            qDebug() << tmProcessName;
        }
    }
}

void TmMonitor::invokeTimepie(){
    qInfo() << "Trying to invoke timepie";
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    qDebug() << "session id is: " << sessionId;
    HANDLE hToken = NULL;
    bool queryResult = WTSQueryUserToken(sessionId, &hToken);
    if (!queryResult) {
        qDebug() << "can not query usertoken";
        DWORD err = GetLastError();
        qDebug() << err;
    }
    STARTUPINFO si = STARTUPINFO();
    si.cb = sizeof(STARTUPINFO);
    PROCESS_INFORMATION pi;
    //bool created = CreateProcessAsUserW(hToken, timepieProgramPath, NULL, NULL, NULL,
    //                                    FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi);
    bool created = CreateProcessAsUserW(hToken, timepieProgramPath, NULL, NULL, NULL,
                                        FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi);
    qDebug() << timepieProgramPath;
    if(!created){
        qCritical() << "COULD NOT CREATE TIMEPIE PROCESS!!!" << " err:" << GetLastError();
    }
}
