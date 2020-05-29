/****************************************************************************
**
** Copyright (C) 2011 Wensheng Wang
** All rights reserved.
** Contact: Wensheng Wang (wenshengwang@gmail.com)
**
**
****************************************************************************/

#include "timepie.h"
#include "pch.h"
#include "ui_dialog.h"
#include "timepieeventfilter.h"
#include "simplecrypt.h"
#include <fstream>
#include <sstream>
#include <odprint.h>
#include <psapi.h> //for GetModuleFileNameEx
#include <WtsApi32.h> // for WTSRegisterSessionNotification
// without below 2 lines, will have min/max identifier not found error
// this is a bug in GdiplusTypes.h, it's reported to be fixed internally at MS, but not released yet
// 20200527
// see: https://developercommunity.visualstudio.com/content/problem/727770/gdiplustypesh-does-not-compile-with-nominmax.html
using std::min;
using std::max;
#include <wrl.h>
#include <wincodec.h>
#include <Wingdi.h>
#include <gdiplus.h>
#include <d3d11.h>
#include <dxgi.h>
#include <KnownFolders.h>
//#include <comdef.h>
#include <system_error>
#include <ScreenGrab.h>
#include <WICTextureLoader.h>
//#include <DirectXHelpers.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment (lib, "Gdiplus.lib")
using namespace Gdiplus;
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
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

SimpleCrypt crypto(Q_UINT64_C(0xa0f1608c385c24a9));

//! [0]
TimePie::TimePie(QWidget *parent): QDialog(parent),  ui(new Ui::TMController)
{
    /*
     * session username
     */
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

    setWindowTitle(tr("ieh.me"));
    
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
    minuteTimer->setInterval(60000);
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

    //see: http://doc.qt.nokia.com/4.7/desktop-screenshot-screenshot-cpp.html
    QPixmap originalPixmap = QPixmap(); // clear image for low memory situations on embedded devices.

    HWND iHwndForegroundWindow = GetForegroundWindow();

    if(iHwndForegroundWindow){
        //Get active window title,
        //if active window is a browser, it will be html doc title
        LPTSTR appTitle = new TCHAR[1023];
        GetWindowText(iHwndForegroundWindow,appTitle,1024);
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

        if(currentApplicationName!=lastApplicationName || currentWindowTitle!=lastWindowTitle){
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
            MONITORINFO monitorInfo;
            monitorInfo.cbSize = sizeof (MONITORINFO);
            GetMonitorInfoW(MonitorFromWindow(iHwndForegroundWindow, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
            LPRECT lpRect=new RECT();
            bool gotRect = GetWindowRect(iHwndForegroundWindow, lpRect);
            if(gotRect){
                bool isFullScreen = (lpRect->left == monitorInfo.rcMonitor.left
                                     && lpRect->right == monitorInfo.rcMonitor.right
                                     && lpRect->top == monitorInfo.rcMonitor.top
                                     && lpRect->bottom == monitorInfo.rcMonitor.bottom);
                if(isFullScreen){
                    odprintf("fullscreen mode detected!");
                    //HWND hwnd = CreateWindowEx(NULL, L"WindowClass", L"", WS_EX_OVERLAPPEDWINDOW,
                    //                           0, 0, lpRect->right - lpRect->left, lpRect->bottom - lpRect->top,
                    //                           NULL, NULL, thisHwnd, NULL);
                    //QWidget *qWidget = new QWidget;
                    //HWND hwnd = (HWND)qWidget->winId();
                    //captureGameFullScreen2(hwnd,
                    //                      fileName,
                    //                      lpRect->right - lpRect->left,
                    //                      lpRect->bottom - lpRect->top);
                    //delete qWidget;
                    captureGameFullScreen3(fileName);
                }else{
                    originalPixmap = screen->grabWindow(QApplication::desktop()->winId(),
                                                        lpRect->left,
                                                        lpRect->top,
                                                        lpRect->right - lpRect->left + 1,
                                                        lpRect->bottom - lpRect->top + 1);
                }
            }else{
                //This works, but it only grab the pane, not window,
                //i.e. no outer frame, not menu, status.
                originalPixmap = screen->grabWindow((WId)iHwndForegroundWindow);
            }
        }
    }else{
        odprintf("Could not get foreground window");
    }



    /*
	//old debug stuff, don't delete
    QString initialPath = QDir::currentPath() + tr("/untitled.") + format;
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                            initialPath,
                                            tr("%1 Files (*.%2);;All Files (*)")
                                            .arg(format.toUpper())
                                            .arg(format));
    */
    //QString fileName = QString("%1/%2.%3").arg(appDataDir).arg(currentTime.currentMSecsSinceEpoch()).arg(format);


	//todo: sometimes have null pixmap, investigate
    if (!originalPixmap.isNull()){
        //save original size
        originalPixmap.save(fileName, "jpg", img_quality);
		//save half size
		//originalPixmap.scaledToHeight(originalPixmap.height()/2).save(fileName, format.toAscii(), img_quality);
    }

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

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
   UINT  num = 0;          // number of image encoders
   UINT  size = 0;         // size of the image encoder array in bytes

   ImageCodecInfo* pImageCodecInfo = NULL;
   GetImageEncodersSize(&num, &size);
   if(size == 0)
      return -1;  // Failure

   pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
   if(pImageCodecInfo == NULL)
      return -1;  // Failure

   GetImageEncoders(num, size, pImageCodecInfo);

   for(UINT j = 0; j < num; ++j)
   {
      if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
      {
         *pClsid = pImageCodecInfo[j].Clsid;
         free(pImageCodecInfo);
         return j;  // Success
      }
   }
   free(pImageCodecInfo);
   return -1;  // Failure
}

void TimePie::captureGameFullScreen(HWND hWnd, QString filename, UINT width, UINT height){
    //wchar_t *filenameL = new wchar_t[filename.length() + 1];
    //filename.toWCharArray(filenameL);
    //filenameL[filename.length()] = 0;
    const wchar_t *wfilename = filename.toStdWString().c_str();

    /*
    Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
    if(FAILED(initialize)){
        odprintf("Failed to initialize");
    }*/

    // create Factory, must be IDXGIFactory2 for CreateSwapChainForHwnd method
    ComPtr<IDXGIFactory2> m_dxgiFactory;
    DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0;
                SUCCEEDED(m_dxgiFactory->EnumAdapters1(
                    adapterIndex,
                    adapter.ReleaseAndGetAddressOf()));
                adapterIndex++)
    {
        DXGI_ADAPTER_DESC1 desc;
        DX::ThrowIfFailed(adapter->GetDesc1(&desc));
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE){
            // Don't select the Basic Render Driver adapter.
            continue;
        }
        //qDebug() << "Direct3D Adapter " << adapterIndex << ": VID:" << desc.VendorId << ", PID:" << desc.DeviceId << " - " << desc.Description;
        odprintf("device: %ls", desc.Description);
        break;
    }
    if(!adapter){
        return;
        //qDebug() << "no Direct3D hardware adapter found";
        //TODO: use gdi
    }
    odprintf("factory and adapter created");


    //UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    UINT creationFlags = 0;
#ifdef QT_DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    static const D3D_FEATURE_LEVEL featureLevels[] ={
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL  m_d3dFeatureLevel;

    HRESULT hr = D3D11CreateDevice(
        adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        creationFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        device.GetAddressOf(),  // Returns the Direct3D device created.
        &m_d3dFeatureLevel,     // Returns feature level of device created.
        context.GetAddressOf()  // Returns the device immediate context.
    );

    if (FAILED(hr)){
        // If the initialization fails, fall back to the WARP device.
        // For more information on WARP, see:
        // http://go.microsoft.com/fwlink/?LinkId=286690
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
            nullptr,
            creationFlags,
            featureLevels,
            _countof(featureLevels),
            D3D11_SDK_VERSION,
            device.GetAddressOf(),
            &m_d3dFeatureLevel,
            context.GetAddressOf()
        );

        if (FAILED(hr)){
            odprintf("Could not created d3d device");
            return;
        }
    }else{
        odprintf("hardware CreateDevice successful");
    }


    ComPtr<ID3D11Device1> m_d3dDevice;
    ComPtr<ID3D11DeviceContext1> m_d3dContext;
    ComPtr<ID3DUserDefinedAnnotation> m_d3dAnnotation;
    DX::ThrowIfFailed(device.As(&m_d3dDevice));
    DX::ThrowIfFailed(context.As(&m_d3dContext));
    DX::ThrowIfFailed(context.As(&m_d3dAnnotation));
    odprintf("d3d device and context are created.");

    ComPtr<IDXGISwapChain1> m_swapChain;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // this is the most common swapchain format
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc.Count = 1; // don't use multi-sampling
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2; // use double buffering to enable flip
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc;
    ZeroMemory(&fullScreenDesc, sizeof(fullScreenDesc));
    fullScreenDesc.RefreshRate.Numerator = 60;
    fullScreenDesc.RefreshRate.Denominator = 1;
    fullScreenDesc.Windowed = FALSE;

    hr = m_dxgiFactory->CreateSwapChainForHwnd(device.Get(),
                                               hWnd,
                                               &swapChainDesc,
                                               &fullScreenDesc,
                                               nullptr,
                                               &m_swapChain);
    if(FAILED(hr)){
        odprintf("Could not create swap chain");
        return;
    }

    BOOL isFullscreen = FALSE;
    m_swapChain->GetFullscreenState(&isFullscreen, nullptr);

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(backBuffer.GetAddressOf()));
    if(SUCCEEDED(hr)){
        hr = SaveWICTextureToFile(context.Get(), backBuffer.Get(), GUID_ContainerFormatJpeg, wfilename);
    }
    DX::ThrowIfFailed(hr);
    return;
#ifdef WILLTRYTHIS
    //https://www.codeproject.com/Articles/5051/Various-methods-for-capturing-the-screen
    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    HWND hDesktopWnd = GetDesktopWindow();
    HDC hDesktopDC = GetDC(hDesktopWnd);
    HDC hCaptureDC = CreateCompatibleDC(hDesktopDC);
    HBITMAP hCaptureBitmap =CreateCompatibleBitmap(hDesktopDC,
                            nScreenWidth, nScreenHeight);
    SelectObject(hCaptureDC,hCaptureBitmap);
    BitBlt(hCaptureDC,0,0,nScreenWidth,nScreenHeight,
           hDesktopDC,0,0,SRCCOPY|CAPTUREBLT);
    SaveCapturedBitmap(hCaptureBitmap); //Place holder - Put your code
                                //here to save the captured image to disk
    ReleaseDC(hDesktopWnd,hDesktopDC);
    DeleteDC(hCaptureDC);
    DeleteObject(hCaptureBitmap);
#else
    //https://stackoverflow.com/questions/3291167/how-can-i-take-a-screenshot-in-a-windows-application
    int x1, y1, x2, y2, w, h;

    // get screen dimensions
    x1  = GetSystemMetrics(SM_XVIRTUALSCREEN);
    y1  = GetSystemMetrics(SM_YVIRTUALSCREEN);
    x2  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    y2  = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    w   = x2 - x1;
    h   = y2 - y1;

    // copy screen to bitmap
    // get device context of the screen
    HDC     hScreen = GetDC(NULL);
    // a device context to put it in
    HDC     hDC     = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, w, h);
    HGDIOBJ old_obj = SelectObject(hDC, hBitmap);
    BOOL    bRet    = BitBlt(hDC, 0, 0, w, h, hScreen, x1, y1, SRCCOPY);

    //use gdi plus to save to png file
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    Bitmap *image = new Bitmap(hBitmap, NULL);
    CLSID myClsId;
    GetEncoderClsid(L"image/png", &myClsId);
    image->Save(L"hello.png", &myClsId, NULL);
    delete image;
    GdiplusShutdown(gdiplusToken);


    // clean up
    SelectObject(hDC, old_obj);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);
    DeleteObject(hBitmap);
#endif
}

void TimePie::captureGameFullScreen2(HWND hWnd, QString filename, UINT width, UINT height){
    wchar_t *wfilename = new wchar_t[filename.length() + 1];
    filename.toWCharArray(wfilename);
    wfilename[filename.length()] = 0;

    /*
    Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
    if(FAILED(initialize)){
        odprintf("Failed to initialize");
    }*/

    // reference:
    //   https://walbourn.github.io/anatomy-of-direct3d-11-create-device/

    UINT creationFlags = 0;
#ifdef QT_DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    static const D3D_FEATURE_LEVEL featureLevels[] ={
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1};

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL  m_d3dFeatureLevel;

    HRESULT hr = D3D11CreateDevice(nullptr,
                                   D3D_DRIVER_TYPE_HARDWARE,
                                   nullptr,
                                   creationFlags,
                                   featureLevels,
                                   _countof(featureLevels),
                                   D3D11_SDK_VERSION,
                                   &device,
                                   &m_d3dFeatureLevel,
                                   &context);

    if(hr == E_INVALIDARG){
        hr = D3D11CreateDevice(nullptr,
                               D3D_DRIVER_TYPE_HARDWARE,
                            nullptr,
                            creationFlags,
                            &featureLevels[1],
                            _countof(featureLevels) - 1,
                            D3D11_SDK_VERSION,
                            &device,
                            &m_d3dFeatureLevel,
                            &context);
    }

    odprintf("level_11_0=%d, got level=%d",
             (int)D3D_FEATURE_LEVEL_11_0, (int)m_d3dFeatureLevel);
    if(m_d3dFeatureLevel < D3D_FEATURE_LEVEL_11_0){
        return;
    }

    ComPtr<ID3D11Device1> m_d3dDevice;
    ComPtr<ID3D11DeviceContext1> m_d3dContext;
    DX::ThrowIfFailed(device.As(&m_d3dDevice));
    DX::ThrowIfFailed(context.As(&m_d3dContext));
    odprintf("d3d device and context are created.");

    ComPtr<IDXGIDevice> dxgiDevice;
    //ComPtr<IDXGIAdapter> dxgiAdapter;
    ComPtr<IDXGIAdapter2> dxgiAdapter2;
    //ComPtr<IDXGIFactory1> dxgiFactory1;
    ComPtr<IDXGIFactory2> dxgiFactory2;

    DX::ThrowIfFailed(device.As(&dxgiDevice));
    //dxgiDevice->GetAdapter(&dxgiAdapter);
    //dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory1));
    //DX::ThrowIfFailed(dxgiFactory1.As(&dxgiFactory2));
    //dxgiAdapter2->GetParent(IID_PPV_ARGS(&dxgiFactory1));
    dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter2));
    dxgiAdapter2->GetParent(IID_PPV_ARGS(&dxgiFactory2));

    ComPtr<IDXGISwapChain1> m_swapChain;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // this is the most common swapchain format
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc.Count = 1; // don't use multi-sampling
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2; // use double buffering to enable flip
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc;
    ZeroMemory(&fullScreenDesc, sizeof(fullScreenDesc));
    fullScreenDesc.RefreshRate.Numerator = 60;
    fullScreenDesc.RefreshRate.Denominator = 1;
    //fullScreenDesc.Windowed = FALSE;
    fullScreenDesc.Windowed = TRUE;

    //ComPtr<IUnknown> unknownDev;
    //DX::ThrowIfFailed(device.As(&unknownDev));
    hr = dxgiFactory2->CreateSwapChainForHwnd(device.Get(),
                                               hWnd,
                                               &swapChainDesc,
                                               &fullScreenDesc,
                                               nullptr,
                                               &m_swapChain);
    if(FAILED(hr)){
        std::string message = std::system_category().message(hr);
        odprintf("Could not create swap chain: %s", message.c_str());
        return;
    }

    BOOL isFullscreen = FALSE;
    m_swapChain->GetFullscreenState(&isFullscreen, nullptr);

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(backBuffer.GetAddressOf()));
    if(SUCCEEDED(hr)){
        odprintf("Trying to save to: %ls", wfilename);
        hr = SaveWICTextureToFile(context.Get(), backBuffer.Get(), GUID_ContainerFormatJpeg, wfilename);
        if(FAILED(hr)){
            std::string message = std::system_category().message(hr);
            odprintf("Could not SaveWICTextureToFile:\n%s", message.c_str());
        }
    }else{
        odprintf("Could not swapChain->GetBuffer.");
    }
}

/*
 * Screen capture using Windows Desktop Duplication API
 * largely based on: https://www.codeproject.com/Tips/1116253/Desktop-screen-capture-on-Windows-via-Windows-Desk
 * save to jpg see: https://github.com/microsoft/DirectXTK/blob/master/Src/ScreenGrab.cpp
 *
 */
void TimePie::captureGameFullScreen3(QString filename)
{
    wchar_t *wfilename = new wchar_t[filename.length() + 1];
    filename.toWCharArray(wfilename);
    wfilename[filename.length()] = 0;

    ComPtr<ID3D11Device> lDevice;
    ComPtr<ID3D11DeviceContext> pContext;
    ComPtr<IDXGIOutputDuplication> lDeskDupl;
    ComPtr<ID3D11Texture2D> lAcquiredDesktopImage;
    ComPtr<ID3D11Texture2D> lGDIImage;
    ComPtr<ID3D11Texture2D> lDestImage;
    DXGI_OUTPUT_DESC lOutputDesc;
    DXGI_OUTDUPL_DESC lOutputDuplDesc;

    D3D_FEATURE_LEVEL gFeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    D3D_FEATURE_LEVEL lFeatureLevel;

    HRESULT hr(E_FAIL);

    // Create device
    hr = D3D11CreateDevice(nullptr,
                    D3D_DRIVER_TYPE_HARDWARE,
                    nullptr,
                    0,
                    gFeatureLevels,
                    ARRAYSIZE(gFeatureLevels),
                    D3D11_SDK_VERSION,
                    &lDevice,
                    &lFeatureLevel,
                    &pContext);

    if (lDevice == nullptr){
        return;
    }

            // Get DXGI device
    ComPtr<IDXGIDevice> lDxgiDevice;

    hr = lDevice->QueryInterface(IID_PPV_ARGS(&lDxgiDevice));

    if (FAILED(hr)){
        return;
    }

    // Get DXGI adapter
    ComPtr<IDXGIAdapter> lDxgiAdapter;

    hr = lDxgiDevice->GetParent(IID_PPV_ARGS(&lDxgiAdapter));
    //dxgiAdapter2->GetParent(IID_PPV_ARGS(&dxgiFactory2));

    if (FAILED(hr)){
        return;
    }

    UINT Output = 0;

    // Get output
    ComPtr<IDXGIOutput> lDxgiOutput;
    hr = lDxgiAdapter->EnumOutputs(Output, &lDxgiOutput);

    if (FAILED(hr)){
        return;
    }

    hr = lDxgiOutput->GetDesc(&lOutputDesc);

    if (FAILED(hr)){
        return;
    }

    // QI for Output 1
    ComPtr<IDXGIOutput1> lDxgiOutput1;

    hr = lDxgiOutput->QueryInterface(IID_PPV_ARGS(&lDxgiOutput1));

    if (FAILED(hr)){
        return;
    }

    // Create desktop duplication
    hr = lDxgiOutput1->DuplicateOutput(lDevice.Get(), &lDeskDupl);

    if (FAILED(hr)){
        return;
    }

    // Create GUI drawing texture
    lDeskDupl->GetDesc(&lOutputDuplDesc);

    D3D11_TEXTURE2D_DESC desc;

    desc.Width = lOutputDuplDesc.ModeDesc.Width;
    desc.Height = lOutputDuplDesc.ModeDesc.Height;
    // this should always be DXGI_FORMAT_B8G8R8A8_UNORM
    //https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api
    desc.Format = lOutputDuplDesc.ModeDesc.Format;
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.MipLevels = 1;
    desc.CPUAccessFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    hr = lDevice->CreateTexture2D(&desc, NULL, &lGDIImage);

    if (FAILED(hr)){
        return;
    }

    if (lGDIImage == nullptr){
        return;
    }

    // Create CPU access texture
    desc.Width = lOutputDuplDesc.ModeDesc.Width;
    desc.Height = lOutputDuplDesc.ModeDesc.Height;
    desc.Format = lOutputDuplDesc.ModeDesc.Format;
    desc.ArraySize = 1;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.MipLevels = 1;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    desc.Usage = D3D11_USAGE_STAGING;

    hr = lDevice->CreateTexture2D(&desc, NULL, &lDestImage);

    if (FAILED(hr)){
        return;
    }

    if (lDestImage == nullptr){
        return;
    }

    ComPtr<IDXGIResource> lDesktopResource;
    DXGI_OUTDUPL_FRAME_INFO lFrameInfo;

    int lTryCount = 4;

    do{
        Sleep(100);

        // Get new frame
        hr = lDeskDupl->AcquireNextFrame(250, &lFrameInfo, &lDesktopResource);

        if (SUCCEEDED(hr)){
            break;
        }

        if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        {
            continue;
        }
        else if (FAILED(hr)){
            break;
        }

    } while (--lTryCount > 0);

    if (FAILED(hr)){
        return;
    }

    // QI for ID3D11Texture2D
    hr = lDesktopResource->QueryInterface(IID_PPV_ARGS(&lAcquiredDesktopImage));

    if (FAILED(hr)){
        return;
    }

    if (lAcquiredDesktopImage == nullptr){
        return;
    }

    // Copy image into GDI drawing texture
    pContext->CopyResource(lGDIImage.Get(), lAcquiredDesktopImage.Get());


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

            DrawIconEx(
                lHDC,
                lCursorPosition.x,
                lCursorPosition.y,
                lCursorInfo.hCursor,
                0,
                0,
                0,
                0,
                DI_NORMAL | DI_DEFAULTSIZE);

            lIDXGISurface1->ReleaseDC(nullptr);
        }

    }

    // Copy image into CPU access texture

    pContext->CopyResource(lDestImage.Get(), lGDIImage.Get());


    // Copy from CPU access texture to bitmap buffer
    D3D11_MAPPED_SUBRESOURCE resource;
    //UINT subresource = D3D11CalcSubresource(0, 0, 0);
    //pContext->Map(lDestImage.Get(), D3D11CalcSubresource(0, 0, 0), D3D11_MAP_READ_WRITE, 0, &resource);
    hr = pContext->Map(lDestImage.Get(),
                                D3D11CalcSubresource(0, 0, 0),
                                D3D11_MAP_READ, 0, &resource);
    if(FAILED(hr)){
        return;
    }
    odprintf("got screenshot in memory, ready to write.");

    /*
    BITMAPINFO	lBmpInfo;

    // BMP 32 bpp
    ZeroMemory(&lBmpInfo, sizeof(BITMAPINFO));
    lBmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    lBmpInfo.bmiHeader.biBitCount = 32;
    lBmpInfo.bmiHeader.biCompression = BI_RGB;
    lBmpInfo.bmiHeader.biWidth = lOutputDuplDesc.ModeDesc.Width;
    lBmpInfo.bmiHeader.biHeight = lOutputDuplDesc.ModeDesc.Height;
    lBmpInfo.bmiHeader.biPlanes = 1;
    lBmpInfo.bmiHeader.biSizeImage = lOutputDuplDesc.ModeDesc.Width  * lOutputDuplDesc.ModeDesc.Height * 4;


    std::unique_ptr<BYTE> pBuf(new BYTE[lBmpInfo.bmiHeader.biSizeImage]);
    UINT lBmpRowPitch = lOutputDuplDesc.ModeDesc.Width * 4;
    BYTE* sptr = reinterpret_cast<BYTE*>(resource.pData);
    BYTE* dptr = pBuf.get() + lBmpInfo.bmiHeader.biSizeImage - lBmpRowPitch;

    UINT lRowPitch = std::min<UINT>(lBmpRowPitch, resource.RowPitch);

    for (size_t h = 0; h < lOutputDuplDesc.ModeDesc.Height; ++h){

        memcpy_s(dptr, lBmpRowPitch, sptr, lRowPitch);
        sptr += resource.RowPitch;
        dptr -= lBmpRowPitch;
    }

    // Save bitmap buffer into the file ScreenShot.bmp
    FILE* lfile = nullptr;

    auto lerr = _wfopen_s(&lfile, wfilename, L"wb");

    if (lerr != 0){
        return;
    }

    if (lfile != nullptr){

        BITMAPFILEHEADER	bmpFileHeader;

        bmpFileHeader.bfReserved1 = 0;
        bmpFileHeader.bfReserved2 = 0;
        bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + lBmpInfo.bmiHeader.biSizeImage;
        bmpFileHeader.bfType = 'MB';
        bmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        fwrite(&bmpFileHeader, sizeof(BITMAPFILEHEADER), 1, lfile);
        fwrite(&lBmpInfo.bmiHeader, sizeof(BITMAPINFOHEADER), 1, lfile);
        fwrite(pBuf.get(), lBmpInfo.bmiHeader.biSizeImage, 1, lfile);

        fclose(lfile);

        //ShellExecute(0, 0, wfilename, 0, 0, SW_SHOW);
    }
    */
    uint64_t imageSize = uint64_t(resource.RowPitch) * uint64_t(desc.Height);
    if (imageSize > UINT32_MAX)
    {
        pContext->Unmap(lDestImage.Get(), 0);
        return;
    }
    odprintf("image size = %lld", (long long)imageSize);

    // because desc.Format is always DXGI_FORMAT_B8G8R8A8_UNORM
    WICPixelFormatGUID pfGuid = GUID_WICPixelFormat32bppBGRA;
    WICPixelFormatGUID targetGuid =  GUID_WICPixelFormat24bppBGR;
    auto pWIC = _GetWIC();

    // Conversion required to write
    ComPtr<IWICBitmap> source;
    hr = pWIC->CreateBitmapFromMemory(desc.Width, desc.Height, pfGuid,
        resource.RowPitch, static_cast<UINT>(imageSize),
        static_cast<BYTE*>(resource.pData), source.GetAddressOf());
    if (FAILED(hr))
    {
        pContext->Unmap(lDestImage.Get(), 0);
        return;
    }

    ComPtr<IWICFormatConverter> FC;
    hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
    if (FAILED(hr))
    {
        pContext->Unmap(lDestImage.Get(), 0);
        return;
    }

    BOOL canConvert = FALSE;
    hr = FC->CanConvert(pfGuid, targetGuid, &canConvert);
    if (FAILED(hr) || !canConvert)
    {
        odprintf("Can not convert from 32bit BGRA to 24bit BGR");
        pContext->Unmap(lDestImage.Get(), 0);
        return;
    }

    hr = FC->Initialize(source.Get(), targetGuid, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr))
    {
        pContext->Unmap(lDestImage.Get(), 0);
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

    hr = frame->SetSize(desc.Width, desc.Height);
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

    WICRect rect = { 0, 0, static_cast<INT>(desc.Width), static_cast<INT>(desc.Height) };
    hr = frame->WriteSource(FC.Get(), &rect);

    pContext->Unmap(lDestImage.Get(), 0);

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
