/****************************************************************************
**
** Copyright (C) 2011 Wensheng Wang
** All rights reserved.
** Contact: Wensheng Wang (wenshengwang@gmail.com)
**
**
****************************************************************************/

#include "timepie.h"
#include "ui_dialog.h"
#include "timepieeventfilter.h"
#include "simplecrypt.h"
#include <fstream>
#include <sstream>
#include <odprint.h>

std::ofstream timepie_logfile;
void TimepieLoggingHandler(QtMsgType type, const QMessageLogContext &, const QString &msg);
bool IsUserAdmin(wchar_t *uname);

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
        //qDebug()<<"title:"<<currentWindowTitle.toUtf8().data();

        //Get active window application filename
        //e.g. if active window is firefox, it will be "firefox.exe"
        TCHAR *appName = new TCHAR[512];  // for GetModuleFileNameExW
        DWORD procId=0;
        GetWindowThreadProcessId(iHwndForegroundWindow,&procId);

        HANDLE handle = OpenProcess(1040,0,procId);
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

            LPRECT lpRect=new RECT();
            int x, y, w, h;
            if(GetWindowRect(iHwndForegroundWindow,lpRect)){
                x = lpRect->left;
                y = lpRect->top;
                w = lpRect->right - lpRect->left + 1;
                h = lpRect->bottom - lpRect->top + 1;
                originalPixmap = screen->grabWindow(QApplication::desktop()->winId(),
                                                 x,y,w,h);
            }else{
                //This works, but it only grab the pane, not window,
                //i.e. no outer frame, not menu, status.
                originalPixmap = screen->grabWindow((WId)iHwndForegroundWindow);
            }
        }
    }


	//todo: should we use jpg?
    QString format = "jpg"; //png, jpg, bmp, can not do gif write
    int img_quality = 25;  //default -1, 0:small compressed, 100:large uncompressed

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

    QString fileName = QString("%1\\%2.%3").arg(screenshotPath).arg(currentTime.toTime_t()).arg(format);

	//todo: sometimes have null pixmap, investigate
    if (!originalPixmap.isNull()){
        //save original size
        originalPixmap.save(fileName, format.toStdString().c_str(), img_quality);
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
