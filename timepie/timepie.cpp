/****************************************************************************
**
** Copyright (C) 2011 Wensheng Wang
** All rights reserved.
** Contact: Wensheng Wang (wenshengwang@gmail.com)
**
**
****************************************************************************/

#include "pch.h"
#include "timepie.h"
#include "ui_dialog.h"
#include "timepieeventfilter.h"
#include "simplecrypt.h"
#include <fstream>
#include <sstream>
#include <odprint.h>
#include <psapi.h> //for GetModuleFileNameEx
#include <WtsApi32.h> // for WTSRegisterSessionNotification

#include <wincodec.h>
#include <Wingdi.h>
//#include <d3d11.h>
//#include <dxgi.h>
#include <KnownFolders.h>
#include <shellapi.h>
#include <system_error>
#include <ScreenGrab.h>
#include <WICTextureLoader.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;
using namespace DirectX;

namespace DirectX
{
    extern bool _IsWIC2() noexcept;
    extern IWICImagingFactory* _GetWIC() noexcept;
}

std::ofstream timepie_logfile;
void TimepieLoggingHandler(QtMsgType type, const QMessageLogContext &, const QString &msg);
bool IsUserAdmin(wchar_t *uname);

SimpleCrypt crypto(Q_UINT64_C(0xa0f1608c385c24a9));

//! [0]
TimePie::TimePie(QWidget *parent):
    QDialog(parent),
    ui(new Ui::TMController),
    ddaInitialized(false)
{
    /*
     * session username
     */
    lastScreenShotTime = 0;
    wchar_t currentUsername[256];
    DWORD sizeUserName = sizeof(currentUsername);
    GetUserNameW(currentUsername, &sizeUserName);

    //check Admin
    sessionUserIsAdmin = IsUserAdmin(currentUsername);
    if(sessionUserIsAdmin){
        odprintf("user IS admin");
    }else{
        odprintf("user NOT admin");
    }

    QSettings settings;

    /*
     * set C:\ProgramData\TimePie as progDataDir
     */
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    progDataDir = env.value("ALLUSERSPROFILE");
    progDataDir.append("\\TimePie");


    /*
     * session username base64 encoded
     */
    sessionUsername = QString::fromWCharArray(currentUsername);
    //sessionUsername = env.value("USERNAME");  // for Mac/Linux will be USER
    odprintf("session username is: %s", sessionUsername.toStdString().c_str());
    QString b64Username = QString::fromLatin1(sessionUsername.toUtf8().toBase64());

    /*
     * write process id to file, filename is b64encoded username + ".pid"
     */
    QString pidFilePath = QString("%1\\%2.pid").arg(progDataDir).arg(b64Username);
    DWORD pid = GetCurrentProcessId();
    std::ofstream pidFile;
    pidFile.open(pidFilePath.toStdString().c_str(), std::ofstream::out);
    pidFile << pid;
    pidFile.close();

    /*
     * write logfile, filename is b64encoded username + "_tm.log"
     * if we do qInstallMessageHandler, the program crash in release mode
     * so we only do this in debug
     * so no logfile in release
     */
#ifdef QT_DEBUG
    QString logpath = QString("%1\\%2_tm.log").arg(progDataDir).arg(b64Username);
    timepie_logfile.open(logpath.toStdString().c_str(), std::fstream::app);
    // if we have DebugView from sysinternal running, the program can not open output and won't start
    // so don't use DebugView in Debug
    qInstallMessageHandler(TimepieLoggingHandler);
#endif

    /*
     * register hotkey ctrl+shift+m to bring up setting window
     * this is the only way to bring out settings window as double-click and context menu
     *   are removed in release
     */
    wchar_t pszConsoleTitle[1024];
    wsprintf(pszConsoleTitle,L"%d/%d", GetTickCount(), GetCurrentProcessId());
    SetConsoleTitle(pszConsoleTitle);
    hwndFound=FindWindow(NULL, pszConsoleTitle);
    RegisterHotKey(hwndFound,                // Set the system identifier of the widget window that will handle the HotKey
                   100,                      // Set identifier HotKey
                   MOD_CONTROL | MOD_SHIFT,  // Set modifiers
                   'M');                     // HotKey
    TimePieEventFilter *myfilter=new TimePieEventFilter;
    myfilter->setup(this);
    QCoreApplication::instance()->installNativeEventFilter(myfilter);

    /*
     * some set up
     */
    lastUploadTime = 0;
    lastApplicationName = QString();
    lastWindowTitle = QString();
    netManager = new QNetworkAccessManager;
    ui->setupUi(this);
    ui->confirmedLabel->hide();

    /*
     * get config file.
     * config file is only saved when any user entered correct ieh.me username and apikey
     *   and was verified by ieh.me server
     * so if it exists, it's valid, the file is encrypted by simplecrypt
     */
    configFilePath = QString(progDataDir);
    configFilePath.append(QDir::separator()).append("timeout.cfg");
    configIsOK = false;
    if(getConfigFromFile()){
        ui->emailEdit->setText(userEmail);
        ui->apikeyEdit->setText(userApiKey);
        ui->computernameEdit->setText(computerName);
        configIsOK = true;
        // here even though config exist, therefore valid, we still verify with server
        //  so we don't serve a user we have canceled.
        authWithWebServer();
    }else{
        ui->computernameEdit->setText(QHostInfo::localHostName());
        ui->isrunningLabel->setText("Please enter your email and API key");
    }

    createTrayIcon();
    trayIcon->show();

    setWindowTitle(tr(APPLICATION_NAME));
    
    //create file-system watcher
    fswatcher = new QFileSystemWatcher;
    fswatcher->addPath(QDir::currentPath());

    /* in release, when user click icon, do nothing!! */
#ifdef QT_DEBUG
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
#endif

    //connect(fswatcher,SIGNAL(fileChanged(QString)),this,SLOT(show()));

    connect(netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(processWebServerReply(QNetworkReply*)));

    initData();

    //try to take a screenshot every minute
    minuteTimer = new QTimer();
    minuteTimer->setInterval(30000);
    connect(minuteTimer,SIGNAL(timeout()),this,SLOT(shootScreen()));
    minuteTimer->start();
    
    //upload data every half hour(1800 seconds)
    hourTimer = new QTimer();
    //hourTimer->setInterval(1800000);
    hourTimer->setInterval(300000);
    connect(hourTimer,SIGNAL(timeout()),this,SLOT(sendData()));
    hourTimer->start();
    
    notActive = false;
    HWND thisHwnd = (HWND)this->winId();
    WTSRegisterSessionNotification(thisHwnd, NOTIFY_FOR_ALL_SESSIONS);
}


void TimePie::showIfAdmin(){
    if(sessionUserIsAdmin){
        setWindowState( (windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        show();
        activateWindow();
    }
}

void TimePie::initData()
{
    QString dbPath(progDataDir);
    dbPath.append("\\timepie.db");
    //connect to sqlite db "timepie.db" in the installation directory
    //create table `entry` if not already exists
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if(db.open()){
        /* "entry" table:
         *   uname: session username
         *   pname: foreground program name
         *   title: program title
         *   forget: ?
         */
        QString sqlstr = QString("CREATE TABLE IF NOT EXISTS entry(timestamp integer, uname text, pname text, title text, forget integer)");
		QSqlQuery query = QSqlQuery(sqlstr,db);
		query.exec();
        //qDebug()<< db.tables();
        db.close();
    }

    QString shotsDir(progDataDir);
    shotsDir.append("\\shots");
    QDir dir = QDir(shotsDir);
    if(!dir.exists()){
        dir.mkpath(".");
    }
    
}

//! [2]
void TimePie::closeEvent(QCloseEvent *event)
{
    if (trayIcon->isVisible()) {
        hide();
        event->ignore();
    }
}

void TimePie::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
        // ignore single-click
        break;

    case QSystemTrayIcon::DoubleClick:
        show();
        break;

    default:
        ;
    }
}



void TimePie::createTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);

#ifdef QT_DEBUG
    trayIconMenu = new QMenu(this);
    QAction *showHelp = trayIconMenu->addAction(QString::fromLocal8Bit("Help"));
    //QAction *quitAction = trayIconMenu->addAction(QByteArray::fromHex("e98080e587ba").data());
    QAction *quitAction = trayIconMenu->addAction(QString::fromLocal8Bit("Exit"));

    connect(showHelp, SIGNAL(triggered()), this, SLOT(openHelpBrowser()));
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    trayIcon->setContextMenu(trayIconMenu);
#endif

    QIcon thisIcon = QIcon(":/images/speaker.png");
    trayIcon->setIcon(thisIcon);
    setWindowIcon(thisIcon);

    // show fake tooltip
    trayIcon->setToolTip("Enhanced Sound");
}

void TimePie::processWebServerReply(QNetworkReply *serverReply)
{
    if(serverReply->error() != QNetworkReply::NoError)
    {
        ui->isrunningLabel->setText("network error");
        odprintf("network error");
        return;
    }

    QByteArray responseData = serverReply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(responseData);
    QJsonObject jsonObj = json.object();
    QString replyType = jsonObj["type"].toString();
    if(replyType == "auth"){
        QString result = jsonObj["result"].toString();
        odprintf("got auth result: %s", result.toStdString().c_str());
        if(result == "ok"){
            if(!configIsOK){
                saveConfigFile();
            }
            userId = jsonObj["userId"].toInt();
            userStatus = jsonObj["userStatus"].toInt();
            lastUploadTime = jsonObj["lastUploadTime"].toInt();
            //qDebug() << "userId:" << userId << " status:" << userStatus;
            //writeToSharedMemory();
            ui->confirmedLabel->show();
            ui->isrunningLabel->setText("Apikey is verified.");
        }else{
            configIsOK = false;
            ui->confirmedLabel->hide();
        }
    }else if(replyType == "data"){
        if(jsonObj["result"] != "ok"){
            //TODO: what do we do here?
        }
    }else if(replyType == "upload"){
        //odprintf("file: %s was saved on remote server", jsonObj["file"].toString().constData());
        odprintf("file: %s was saved on remote server", jsonObj["file"].toString().toStdString().c_str());
        //QString fileName = QString("%1\\%2.jpg").arg(appDataDir).arg(jsonObj["file"].toString());
        //if(QFileInfo::exists(fileName)){
        //    QFile::remove(fileName);
        //}
    }
    serverReply->deleteLater();
}


void TimePie::saveConfigFile(){
    QFile configFile(configFilePath);
    QJsonObject jsonObj;
    jsonObj.insert("Email", QJsonValue::fromVariant(userEmail));
    jsonObj.insert("ApiKey", QJsonValue::fromVariant(userApiKey));
    jsonObj.insert("Computer", QJsonValue::fromVariant(computerName));
    QJsonDocument json(jsonObj);
    configFile.open(QIODevice::WriteOnly);
    QTextStream outStream(&configFile);
    outStream << crypto.encryptToString(json.toJson());
    configFile.close();
    configIsOK = true;
    ui->confirmedLabel->show();
    odprintf("config file: %s saved", configFilePath.toStdString().c_str());
}

void TimePie::openHelpBrowser()
{
    QDesktopServices::openUrl(QUrl("https://ieh.me/help",QUrl::StrictMode));
}

void TimePie::shootScreen()
{
    if(!configIsOK){
        return;
    }
    QDateTime currentTime = QDateTime::currentDateTime();
    QString currentApplicationName=QString();
    QString currentWindowTitle=QString();
    QString screenshotPath = QString(progDataDir);
    screenshotPath.append("\\shots");
    QScreen *screen = QGuiApplication::primaryScreen();
    QString fileName = QString("%1\\%2.jpg").arg(screenshotPath).arg(currentTime.toTime_t());
    int img_quality = 25;  //default -1, 0:small compressed, 100:large uncompressed

    HWND iHwndForegroundWindow = GetForegroundWindow();
    if(!iHwndForegroundWindow){
        odprintf("Could not get foreground window");
        return;
    }

    //Get active window title,
    //if active window is a browser, it will be html doc title
    LPTSTR appTitle = new TCHAR[1023];
    GetWindowText(iHwndForegroundWindow, appTitle, 1024);
    if(appTitle!=NULL){
        currentWindowTitle = QString::fromUtf16( (ushort*)appTitle );
    }
    odprintf("foregroundWindow: %s",  currentWindowTitle.toStdString().c_str());

    //Get active window application filename
    //e.g. if active window is firefox, it will be "firefox.exe"
    TCHAR *appName = new TCHAR[512];  // for GetModuleFileNameExW
    DWORD procId=0;
    GetWindowThreadProcessId(iHwndForegroundWindow,&procId);

    HANDLE handle = OpenProcess(1040, 0, procId);
    //PROCESS_QUERY_INFORMATION(0x400)|PROCESS_VM_READ(0x10)=0x410=1040

    if(handle){
        if(::GetModuleFileNameExW(handle,NULL,appName,512)){
            currentApplicationName = QString::fromUtf16( (ushort*)appName );
        }
    }
    CloseHandle(handle);
    delete[] appName;
    //qDebug()<<"name:"<<currentApplicationName.toUtf8().data();

    if(currentApplicationName==lastApplicationName && currentWindowTitle==lastWindowTitle){
        ULONGLONG currentTC = GetTickCount64() - lastScreenShotTime;
        if(currentTC < SameAppScreenShotInterval){
            return;
        }
    }else{
        if(db.open()){
            QString sqlstr = QString("insert into entry values('%1','%2','%3','%4',%5)")
                                     .arg(currentTime.toTime_t())
                                     .arg(sessionUsername)
                                     .arg(currentApplicationName)
                                     .arg(currentWindowTitle)
                                     .arg(1);
            //qDebug()<<sqlstr.toUtf8().data();
            odprintf("doing sqlite insert: %s", sqlstr.toUtf8().data());

            QSqlQuery query = QSqlQuery(db);
            bool ret = query.exec(sqlstr);
            if (ret){
                odprintf(" -> success");
                //qDebug()<<query.lastInsertId().toInt() << query.lastError().text();
            }else{
                        odprintf(" -> insert failed");
            }
            db.close();
        }
    }

    unsigned int fullscreenStatus = 0;
    QUERY_USER_NOTIFICATION_STATE pquns;
    SHQueryUserNotificationState(&pquns);
    // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ne-shellapi-query_user_notification_state
    // youtube, powerpoint fullscreen will be QUNS_BUSY
    // but sometimes QUNS_BUSY not really fullscreen
    if(pquns == QUNS_RUNNING_D3D_FULL_SCREEN){
        // game such as: Minecraft, LOL
        odprintf("QUNS_RUNNING_D3D_FULL_SCREEN");
        fullscreenStatus = 1;
    }

    RECT rect;
    if(GetWindowRect(iHwndForegroundWindow, &rect)){
        odprintf("rect left, right, top, bottom = %d, %d, %d, %d",
                 rect.left, rect.right, rect.top, rect.bottom);

    }
    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof (MONITORINFO);
    if(GetMonitorInfoW(MonitorFromWindow(iHwndForegroundWindow, MONITOR_DEFAULTTOPRIMARY), &monitorInfo)){
        odprintf("monitor left, right, top, bottom = %d, %d, %d, %d",
                 monitorInfo.rcMonitor.left,
                 monitorInfo.rcMonitor.right,
                 monitorInfo.rcMonitor.top,
                 monitorInfo.rcMonitor.bottom);
    }

    if(!fullscreenStatus && rect.right){
        if(rect.left == monitorInfo.rcMonitor.left
           && rect.right == monitorInfo.rcMonitor.right
           && rect.top == monitorInfo.rcMonitor.top
           && rect.bottom == monitorInfo.rcMonitor.bottom){
            // Roblox fullscreen etc.
            // if QUNS_BUSY is really fullscreen, status will be over-write to 3
            fullscreenStatus = 2;
        }
    }

    if(fullscreenStatus){
        odprintf("fullscreen mode detected: %d", fullscreenStatus);
        captureFullScreenDDA(fileName);
    }else{
        //https://doc.qt.io/archives/qt-4.8/qt-desktop-screenshot-screenshot-cpp.html
        QPixmap originalPixmap = QPixmap();
        if(rect.right){
            originalPixmap = screen->grabWindow(QApplication::desktop()->winId(),
                                                rect.left,
                                                rect.top,
                                                rect.right - rect.left + 1,
                                                rect.bottom - rect.top + 1);

        }else{
            //This works, but it only grab the pane, not window,
            //i.e. no outer frame, not menu, status.
            originalPixmap = screen->grabWindow((WId)iHwndForegroundWindow);
        }

        if (!originalPixmap.isNull()){
            originalPixmap.save(fileName, "jpg", img_quality);
            //save half size
            //originalPixmap.scaledToHeight(originalPixmap.height()/2).save(fileName, format.toAscii(), img_quality);
        }
    }

    lastScreenShotTime = GetTickCount64();
    lastWindowTitle = currentWindowTitle;
    lastApplicationName = currentApplicationName;
}

void TimePie::sendData()
{
    if(configIsOK == false || lastUploadTime == 0){
        return;
    }


    QString sqlstr = QString("SELECT timestamp, uname, pname, title FROM entry where timestamp >%1").arg(lastUploadTime);
    if(!db.open()){
        return;
    }
    QSqlQuery query = QSqlQuery(sqlstr, db);
    query.exec();
    QJsonArray jsonArray;
    while (query.next()){
        QJsonObject jsonObj;
        jsonObj.insert("time", query.value(0).toInt());
        jsonObj.insert("uname", query.value(1).toString());
        jsonObj.insert("pname", query.value(2).toString());
        jsonObj.insert("title", query.value(3).toString());
        jsonArray.append(jsonObj);
    }
    db.close();

    QJsonObject data;
    data["data"] = jsonArray;
    data["email"] = userEmail;
    data["apikey"] = userApiKey;
    int currentUploadTime = QDateTime::currentDateTime().toTime_t();
    data["last"] = currentUploadTime;
    QJsonDocument json;
    json.setObject(data);

    QUrl url("http://ieh.me/a/data");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json; charset=utf-8"));
    
    //Sending the Request
    netManager->post(req, json.toJson());

    QDir dir(progDataDir + "\\shots");
    dir.setFilter(QDir::Files|QDir::NoSymLinks);
    const QFileInfoList fileInfoList = dir.entryInfoList();
    foreach(const QFileInfo& fi, fileInfoList){
        if((unsigned)fi.baseName().toInt() < lastUploadTime){
            QFile::remove(fi.absoluteFilePath());
        }else{
            uploadFile(fi);
        }
    }
    lastUploadTime = currentUploadTime;
}

void TimePie::uploadFile(const QFileInfo fi){
    QString uploadAddress = QString("http://ieh.me/a/upload/%1").arg(userId);
    QUrl testUrl(uploadAddress);
    QNetworkRequest request(testUrl);
    
    QString fileName = fi.absoluteFilePath();
    /*
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,    QVariant("image/jpeg"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"upload\"; filename=\""+ fi.baseName() + "\""));
    QFile *file = new QFile(fileName);
    file->open(QIODevice::ReadOnly);
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);
    netManager->post(request, multiPart);
    */
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)){
        return;
    }
    QString boundary = "------------whateverhahaha";
    QByteArray datas(QString("--" + boundary + "\r\n").toLatin1());
    datas += "Content-Disposition: form-data; name=\"upload\"; filename=\"";
    datas += fi.baseName();
    datas += "\"\r\nContent-Type: image/jpeg\r\n\r\n";
    datas += file.readAll();
    file.close();
    datas += "\r\n";
    datas += QString("--" + boundary + "\r\n").toLatin1();
    datas += "Content-Disposition: form-data; name=\"upload\"\r\n\r\n";
    datas += "Uploader\r\n";
    datas += QString("--" + boundary + "--\r\n").toLatin1();
    
    request.setRawHeader(QString("Content-Type").toLatin1(), QString("multipart/form-data; boundary=" + boundary).toLatin1());
    request.setRawHeader(QString("Content-Length").toLatin1(), QString::number(datas.length()).toLatin1());
    netManager->post(request,datas);
}

void TimepieLoggingHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    std::ostringstream ostrstream;
//    wchar_t ws[2048];
    switch (type) {
    case QtInfoMsg:
        ostrstream << QTime::currentTime().toString().toLocal8Bit().constData() << " Info: " << msg.toLocal8Bit().constData() << "| in:" << context.file << context.line << context.function << "\n";
        break;
    case QtDebugMsg:
        ostrstream << QTime::currentTime().toString().toLocal8Bit().constData() << " Debug: " << msg.toLocal8Bit().constData() << "| in:" << context.file << " line:" << context.line << " func:" << context.function << "\n";
        break;
    case QtCriticalMsg:
        ostrstream << QTime::currentTime().toString().toLocal8Bit().constData() << " Critical: " << context.file << context.line << context.function << "\n";
        break;
    case QtWarningMsg:
        ostrstream << QTime::currentTime().toString().toLocal8Bit().constData() << " Warning: " << context.file << context.line << context.function << "\n";
        break;
    case QtFatalMsg:
        ostrstream << QTime::currentTime().toString().toLocal8Bit().constData() <<  " Fatal: " << msg.toLocal8Bit().constData() << "\n";
        abort();
     }
    timepie_logfile << ostrstream.str();
    //timepie_logfile.flush();
//#ifdef QT_NO_DEBUG
//    swprintf(ws, 2048, L"hs", ostrstream.str().c_str());
//    OutputDebugString(ws);
//#endif
}

bool TimePie::nativeEvent(const QByteArray &eventType, void *message, long *result){
    Q_UNUSED(eventType);
    Q_UNUSED(result);
    MSG *msg = static_cast<MSG*>(message);
    switch(msg->message){
    case WTS_SESSION_LOCK:
        minuteTimer->stop();
        hourTimer->stop();  
        break;
    case WTS_SESSION_UNLOCK:
        minuteTimer->start();
        hourTimer->start();
        break;
    default:
        break;
    }
    return false;
}

bool TimePie::getConfigFromFile(){
    QFile configFile(configFilePath);
    if(!configFile.exists()){
        odprintf("timeout.cfg doesn't exist.");
        return false;
    }
    configFile.open(QIODevice::ReadOnly);
    QString val = configFile.readAll();
    configFile.close();
    QByteArray decrypted = crypto.decryptToByteArray(val);
    if(crypto.lastError()){
        return false;
    }
    QJsonDocument configJson = QJsonDocument::fromJson(decrypted);
    QJsonObject configObj = configJson.object();
    userEmail = configObj["Email"].toString();
    userApiKey = configObj["ApiKey"].toString();
    //userStatus = configObj["userStatus"].toString();
    computerName = configObj["Computer"].toString();
    return true;
}

void TimePie::on_confirmButton_clicked()
{
    userEmail = ui->emailEdit->text();
    userApiKey = ui->apikeyEdit->text();
    computerName = ui->computernameEdit->text();

    authWithWebServer();
}

void TimePie::authWithWebServer(){
    QUrl url = QString("http://ieh.me/a/auth");
    QNetworkRequest req(url);
    QJsonDocument json;
    QJsonObject data;
    data["email"] = userEmail;
    data["apikey"] = userApiKey;
    json.setObject(data);
    req.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json; charset=utf-8"));
    //req.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(jsonPost.size()));
    netManager->post(req, QJsonDocument(data).toJson());
}

void TimePie::on_closeButton_clicked()
{
    hide();
}

bool IsUserAdmin(wchar_t* uname){
    bool result;
    DWORD rc;
    USER_INFO_1 *info;
    rc = NetUserGetInfo(NULL, uname, 1, (byte **) &info);
    if(rc!= NERR_Success){
        return false;
    }
    result = info->usri1_priv == USER_PRIV_ADMIN;
    NetApiBufferFree(info);
    return result;
}


/*
 * Screen capture using Windows Desktop Duplication API
 * largely based on: https://www.codeproject.com/Tips/1116253/Desktop-screen-capture-on-Windows-via-Windows-Desk
 * save to jpg see: https://github.com/microsoft/DirectXTK/blob/master/Src/ScreenGrab.cpp
 * also: https://stackoverflow.com/questions/51613903/desktopduplication-api-produces-black-frames-while-certain-applications-are-in-f
 * and: https://stackoverflow.com/questions/46145057/desktop-duplication-directx-screen-capture-fails-to-deliver-screen-updates
 * and: https://github.com/sskodje/ScreenRecorderLib - internal_recorder.cpp
 */
void TimePie::captureFullScreenDDA(QString filename)
{
    wchar_t *wfilename = new wchar_t[filename.length() + 1];
    filename.toWCharArray(wfilename);
    wfilename[filename.length()] = 0;
    const bool catchCursor = false;

    odprintf("screenshot filename: %ls", wfilename);
    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext>  pContext;
    ComPtr<IDXGIOutputDuplication> pDeskDup;
    D3D11_TEXTURE2D_DESC dDesc;
    HRESULT hr(E_FAIL);

    odprintf("Start initializing DDA");

    D3D_FEATURE_LEVEL gFeatureLevels =  D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL lFeatureLevel;

    /*
    IDXGIFactory1 *dFactory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dFactory));
    if(FAILED(hr)){
        odprintf("could not create dfactory");
        return;
    }
    IDXGIAdapter *dAdapter;
    hr = dFactory->EnumAdapters(0, &dAdapter);
    if(FAILED(hr)){
        odprintf("could not enumerate device");
        return;
    }

    hr = D3D11CreateDevice(dAdapter,
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                0,
                &gFeatureLevels,
                1,
                D3D11_SDK_VERSION,
                &pDevice,
                &lFeatureLevel,
                &pContext);
                */

    // Create device
    hr = D3D11CreateDevice(nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                0,
                &gFeatureLevels,
                1,
                D3D11_SDK_VERSION,
                &pDevice,
                &lFeatureLevel,
                &pContext);

    if(FAILED(hr)){
        std::string message = std::system_category().message(hr);
        odprintf("Could not create device: 0X%08lx\n%s", hr, message.c_str());
        return;
    }

    if (pDevice == nullptr){
        odprintf("Could not create device");
        return;
    }
    // Get DXGI device
    ComPtr<IDXGIDevice> lDxgiDevice;
    hr = pDevice->QueryInterface(IID_PPV_ARGS(&lDxgiDevice));
    if (FAILED(hr)){
        odprintf("Could not cat DXGI device");
        return;
    }

    // Get DXGI adapter
    ComPtr<IDXGIAdapter> lDxgiAdapter;
    hr = lDxgiDevice->GetParent(IID_PPV_ARGS(&lDxgiAdapter));
    if (FAILED(hr)){
        odprintf("Could not cat DXGI adapter");
        return;
    }
    lDxgiDevice.Reset();

    // Get output
    UINT Output = 0;
    ComPtr<IDXGIOutput> lDxgiOutput;
    hr = lDxgiAdapter->EnumOutputs(Output, &lDxgiOutput);
    if (FAILED(hr)){
        odprintf("Could not cat DXGI output");
        return;
    }

    /*
    DXGI_OUTPUT_DESC lOutputDesc;
    hr = lDxgiOutput->GetDesc(&lOutputDesc);
    if (FAILED(hr)){
        return false;
    }
    */
    lDxgiAdapter.Reset();

    ComPtr<IDXGIOutput1> lDxgiOutput1;
    hr = lDxgiOutput->QueryInterface(IID_PPV_ARGS(&lDxgiOutput1));
    if (FAILED(hr)){
        odprintf("Could not cat DXGI output 1");
        return;
    }
    lDxgiOutput.Reset();

    // Create desktop duplication
    hr = lDxgiOutput1->DuplicateOutput(pDevice.Get(), &pDeskDup);
    if (FAILED(hr)){
        std::string message = std::system_category().message(hr);
        odprintf("Could not create DD output: 0X%08lx\n%s", hr, message.c_str());
        return;
    }
    lDxgiOutput1.Reset();

    DXGI_OUTDUPL_DESC lOutputDuplDesc;
    // Create GUI drawing texture
    pDeskDup->GetDesc(&lOutputDuplDesc);


    dDesc.Width = lOutputDuplDesc.ModeDesc.Width;
    dDesc.Height = lOutputDuplDesc.ModeDesc.Height;
    // this should always be DXGI_FORMAT_B8G8R8A8_UNORM
    //https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api
    dDesc.Format = lOutputDuplDesc.ModeDesc.Format;
    dDesc.ArraySize = 1;
    //dDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
    dDesc.BindFlags = 0;
    //dDesc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
    dDesc.MiscFlags = 0;
    dDesc.SampleDesc.Count = 1;
    dDesc.SampleDesc.Quality = 0;
    dDesc.MipLevels = 1;
    dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    //dDesc.Usage = D3D11_USAGE_DEFAULT; // read/write
    dDesc.Usage = D3D11_USAGE_STAGING; //support transfer/copy from GPU to CPU
    odprintf("DDA initiallized");

    ComPtr<IDXGIResource> lDesktopResource;

    DWORD startTicks = GetTickCount();
    int attempted = 0;
    while(true){
        DXGI_OUTDUPL_FRAME_INFO lFrameInfo;
        //pDeskDup->ReleaseFrame();
        odprintf("before 1st acquire next fram");
        hr = pDeskDup->AcquireNextFrame(500, &lFrameInfo, &lDesktopResource);
        odprintf("after 1st acquire next fram");

        if(SUCCEEDED(hr) && lFrameInfo.TotalMetadataBufferSize > 0 && lFrameInfo.LastPresentTime.QuadPart > 0){
            break;
        }

        pDeskDup->ReleaseFrame();
        attempted++;
        if(GetTickCount() - startTicks > 2000 || attempted > 10){
            odprintf("Timed out after %d attempts", attempted);
            break;
        }
    }

    /*
    hr = pDeskDup->AcquireNextFrame(500, &lFrameInfo, &lDesktopResource);
    if(FAILED(hr)){
        std::string message = std::system_category().message(hr);
        odprintf("AcquireNextFrame failed: 0X%08lx\n%s", hr, message.c_str());
        if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
            // Access will be lost when changing from/to fullscreen mode(any application);
            // when this happens we need to reaquirce the outputdup
            odprintf("DXGI ACCESS_LOST/INVALID_CALL - trying to re-init DDA");
            ddaInitialized = false;
            initDDA();
            if(!ddaInitialized){
                return;
            }
        }
        DWORD startTicks = GetTickCount();
        int attempted = 0;
        do{
            pDeskDup->ReleaseFrame();
            hr = pDeskDup->AcquireNextFrame(500, &lFrameInfo, &lDesktopResource);
            if(FAILED(hr)){
                if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
                    // this should not happen but just in case
                    ddaInitialized = false;
                    initDDA();
                    if(!ddaInitialized){
                        return;
                    }
                }
            }
            attempted++;
            if(GetTickCount() - startTicks > 2000){
                odprintf("Timed out after %d attempts", attempted);
                break;
            }
        } while(lFrameInfo.TotalMetadataBufferSize <= 0 ||
                lFrameInfo.LastPresentTime.QuadPart <= 0);
    }
    */

    if (FAILED(hr)){
        std::string message = std::system_category().message(hr);
        odprintf("AcquireNextFrame failed: 0X%08lx\n%s", hr, message.c_str());
        return;
    }

    ComPtr<ID3D11Texture2D> lAcquiredDesktopImage;
    hr = lDesktopResource->QueryInterface(IID_PPV_ARGS(&lAcquiredDesktopImage));
    //lDesktopResource->Release();
    //lDesktopResource = nullptr;
    if (FAILED(hr) || lAcquiredDesktopImage == nullptr){
        odprintf("Failed to acuqire texture from resource");
        pDeskDup->ReleaseFrame();
        return;
    }

    ComPtr<ID3D11Texture2D> lGDIImage;
    ComPtr<ID3D11Texture2D> lDestImage;

    hr = pDevice->CreateTexture2D(&dDesc, NULL, &lGDIImage);
    if (FAILED(hr) || lGDIImage == nullptr){
        odprintf("Failed to copy image data to access texture");
        pDeskDup->ReleaseFrame();
        return;
    }

    // Copy image into GDI drawing texture
    pContext->CopyResource(lGDIImage.Get(), lAcquiredDesktopImage.Get());

    D3D11_MAPPED_SUBRESOURCE resource;

    if(catchCursor){
        // Draw cursor image into GDI drawing texture
        ComPtr<IDXGISurface1> lIDXGISurface1;
        hr = lGDIImage->QueryInterface(IID_PPV_ARGS(&lIDXGISurface1));
        if (FAILED(hr)){
            return;
        }

        CURSORINFO lCursorInfo = { 0 };
        lCursorInfo.cbSize = sizeof(lCursorInfo);
        auto lBoolres = GetCursorInfo(&lCursorInfo);

        if (lBoolres == TRUE){
            if (lCursorInfo.flags == CURSOR_SHOWING){
                auto lCursorPosition = lCursorInfo.ptScreenPos;
                auto lCursorSize = lCursorInfo.cbSize;
                HDC  lHDC;
                lIDXGISurface1->GetDC(FALSE, &lHDC);

                DrawIconEx( lHDC,
                    lCursorPosition.x,
                    lCursorPosition.y,
                    lCursorInfo.hCursor,
                    0, 0, 0, 0,
                    DI_NORMAL | DI_DEFAULTSIZE);

                lIDXGISurface1->ReleaseDC(nullptr);
            }
        }

        // Copy image into CPU access texture
        hr = pDevice->CreateTexture2D(&dDesc, NULL, &lDestImage);
        if (FAILED(hr) || lDestImage == nullptr){
            return;
        }

        pContext->CopyResource(lDestImage.Get(), lGDIImage.Get());
        pContext->Map(lDestImage.Get(), 0, D3D11_MAP_READ, 0, &resource);
    }else{
        pContext->Map(lGDIImage.Get(), 0, D3D11_MAP_READ, 0, &resource);

    }

    auto unmapTextureImage = [&](){
        if(catchCursor){
            pContext->Unmap(lDestImage.Get(), 0);
        }else{
            pContext->Unmap(lGDIImage.Get(), 0);
        }
        pDeskDup->ReleaseFrame();
    };

    uint64_t imageSize = uint64_t(resource.RowPitch) * uint64_t(dDesc.Height);
    //for 1080p this is: 1920*1080*4=8294400
    odprintf("image size = %lld", (long long)imageSize);
    if (imageSize > UINT32_MAX){
        unmapTextureImage();
        return;
    }

    // because dDesc.Format is always DXGI_FORMAT_B8G8R8A8_UNORM
    WICPixelFormatGUID pfGuid = GUID_WICPixelFormat32bppBGRA;
    WICPixelFormatGUID targetGuid =  GUID_WICPixelFormat24bppBGR;
    auto pWIC = _GetWIC();

    // Conversion required to write
    ComPtr<IWICBitmap> source;
    hr = pWIC->CreateBitmapFromMemory(dDesc.Width, dDesc.Height, pfGuid,
        resource.RowPitch, static_cast<UINT>(imageSize),
        static_cast<BYTE*>(resource.pData), source.GetAddressOf());
    if (FAILED(hr)) {
        unmapTextureImage();
        return;
    }

    ComPtr<IWICFormatConverter> FC;
    hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
    if (FAILED(hr)){
        unmapTextureImage();
        return;
    }

    BOOL canConvert = FALSE;
    hr = FC->CanConvert(pfGuid, targetGuid, &canConvert);
    if (FAILED(hr) || !canConvert){
        odprintf("Can not convert from 32bit BGRA to 24bit BGR");
        unmapTextureImage();
        return;
    }

    hr = FC->Initialize(source.Get(), targetGuid, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)){
        unmapTextureImage();
        return;
    }

    odprintf("Creating stream and prepare jpeg encoder");

    ComPtr<IWICStream> stream;
    hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr)){
        return;
    }

    hr = stream->InitializeFromFilename(wfilename, GENERIC_WRITE);
    if (FAILED(hr)){
        return;
    }

    //auto_delete_file_wic delonfail(stream, fileName);
    ComPtr<IWICBitmapEncoder> encoder;
    hr = pWIC->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, encoder.GetAddressOf());
    if (FAILED(hr)){
        return;
    }

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)){
        return;
    }

    odprintf("Encoder initialized.");

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(frame.GetAddressOf(), props.GetAddressOf());
    if (FAILED(hr)){
        return;
    }

    hr = frame->Initialize(props.Get());
    if(FAILED(hr)){
        odprintf("Could not initialized encoder frame");
        return;
    }

    hr = frame->SetSize(dDesc.Width, dDesc.Height);
    if(FAILED(hr)){
        return;
    }

    frame->SetResolution(72,72);
    hr = frame->SetPixelFormat(&targetGuid);
    if(FAILED(hr)){
        odprintf("Could not set pixel format");
    }
    // Encode WIC metadata
    ComPtr<IWICMetadataQueryWriter> metawriter;
    if (SUCCEEDED(frame->GetMetadataQueryWriter(metawriter.GetAddressOf()))){
        PROPVARIANT value;
        PropVariantInit(&value);

        value.vt = VT_LPSTR;
        value.pszVal = const_cast<char*>("DirectXTK");

        // Set Software name
        (void)metawriter->SetMetadataByName(L"System.ApplicationName", &value);
        // Set EXIF Colorspace of sRGB
        value.vt = VT_UI2;
        value.uiVal = 1;
        (void)metawriter->SetMetadataByName(L"System.Image.ColorSpace", &value);
    }

    WICRect rect = { 0, 0, static_cast<INT>(dDesc.Width), static_cast<INT>(dDesc.Height) };
    hr = frame->WriteSource(FC.Get(), &rect);

    unmapTextureImage();

    if (FAILED(hr)){
        std::string message = std::system_category().message(hr);
        odprintf("Could not writeSource:\n%s", message.c_str());
        return;
    }

    hr = frame->Commit();
    if (FAILED(hr)){
        odprintf("Failed to commit frame");
        return;
    }

    hr = encoder->Commit();
    if (FAILED(hr)){
        return;
    }

    //delonfail.clear();
}
