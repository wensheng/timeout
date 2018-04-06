#include "tmmonitor.h"
#include "odprint.h"
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
    odprintf("In TmMonitor()");

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    progDataDir =  env.value("ALLUSERSPROFILE");
    progDataDir.append("\\TimePie");

    QString logPath = QString(progDataDir);
    logPath.append("\\tm_monitor_service.log");
    logfile.open(logPath.toStdString(), std::ofstream::app);
    qInstallMessageHandler(SimpleLoggingHandler);

    //QString appDirPath = QCoreApplication::applicationDirPath();
    //QString timepiePath = "G:\\bitbucket\\timeout2\\build-timepie-Desktop_Qt_5_10_1_MSVC2015_32bit-Debug\\debug\\timepie.exe";
    QString timepiePath = QCoreApplication::applicationDirPath();
    timepiePath.append("\\timepie.exe");
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
    odprintf("getting userToken ...");
    result = WTSQueryUserToken(sessionId, &hToken);
    if (!result) {
        odprintf("can not query usertoken [err=%ld]", GetLastError());
        CloseHandle(hToken);
        return false;
    }

    DWORD tokenInfoLength = 0;
    //first call gets length of TokenInformation
    odprintf("GetTokenInformation  length ...");
    result = GetTokenInformation(hToken,
                                 TokenUser,
                                 NULL,
                                 0,
                                 &tokenInfoLength);
    if(!result && GetLastError() != ERROR_INSUFFICIENT_BUFFER){
        odprintf("could not get token info length, err:%ld", GetLastError());
        CloseHandle(hToken);
        return false;
    }

    std::vector<BYTE> buffer;
    buffer.resize(tokenInfoLength);
    PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(&buffer[0]);
    odprintf("GetTokenInformation again for real ...");
    result = GetTokenInformation(hToken,
                                 TokenUser,
                                 pTokenUser,
                                 tokenInfoLength,
                                 &tokenInfoLength);
    if(!result){
        odprintf("could not get user tokeninfo, err:%ld", GetLastError());
        CloseHandle(hToken);
        return false;
    }
    odprintf("current session user sid: %ld", pTokenUser->User.Sid);
    if(!IsValidSid(pTokenUser->User.Sid)){
        odprintf("user sid not valid");
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
        odprintf("could not look up sid");
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
    odprintf("current user is %s", activeUserName.toStdString().c_str());
    QString pidFilePath = QString("%1\\%2.pid").arg(progDataDir).arg(QString::fromLatin1(activeUserName.toUtf8().toBase64()));
    odprintf("pidfile is: %s", pidFilePath.toStdString().c_str());
    QFile pidFile(pidFilePath);
    if(!pidFile.exists()){
        qDebug() << "pid doesn't exist";
        invokeTimepie();
    }else{
        pidFile.open(QIODevice::ReadOnly);
        DWORD tmpid = atol(pidFile.readAll());
        pidFile.close();
        odprintf("pid exist: %ld", tmpid);
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
    odprintf("Trying to invoke timepie");
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    odprintf("session id is: %ld", sessionId);
    HANDLE hToken = NULL;
    bool queryResult = WTSQueryUserToken(sessionId, &hToken);
    if (!queryResult) {
        odprintf("can not query usertoken, err:%ld", GetLastError());
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
        //qCritical() << "COULD NOT CREATE TIMEPIE PROCESS!!!" << " err:" << GetLastError();
        odprintf("COULD NOT CREATE TIMEPIE PROCESS!!! err:%ld", GetLastError());
    }
}
