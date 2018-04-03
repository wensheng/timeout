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

std::ofstream timepie_logfile;
void TimepieLoggingHandler(QtMsgType type, const QMessageLogContext &, const QString &msg);

//! [0]
TimePie::TimePie(QWidget *parent): QDialog(parent),  ui(new Ui::TMController)
{
#ifdef _DEBUG
	QString logpath(QDir::home().path());
    logpath.append(QDir::separator()).append("timepied.log");
    //timepie_logfile.open(QDir::toNativeSeparators(logpath).toStdString().c_str(), std::ios::app|std::ios::binary);
    timepie_logfile.open(QDir::toNativeSeparators(logpath).toStdString().c_str(), std::ios::app);
    //qInstallMessageHandler(TimepieLoggingHandler);
#endif
    //installEventFilter(this);
    wchar_t pszConsoleTitle[1024];
    wsprintf(pszConsoleTitle,L"%d/%d", GetTickCount(), GetCurrentProcessId());
    SetConsoleTitle(pszConsoleTitle);
    hwndFound=FindWindow(NULL, pszConsoleTitle);
    RegisterHotKey(hwndFound,   // Set the system identifier of the widget window that will handle the HotKey
                   100,                         // Set identifier HotKey
                   MOD_CONTROL | MOD_SHIFT,         // Set modifiers
                   'M');                        // HotKey
    TimePieEventFilter *myfilter=new TimePieEventFilter;
    myfilter->setup(this);
    QCoreApplication::instance()->installNativeEventFilter(myfilter);

    lastUploadTime = 0;
    lastApplicationName = QString();
    lastWindowTitle = QString();
    netManager = new QNetworkAccessManager;


    ui->setupUi(this);

    createTrayIcon();
    trayIcon->show();

    setWindowTitle(tr("Time Pie"));
    
    //create file-system watcher
    fswatcher = new QFileSystemWatcher;
    fswatcher->addPath(QDir::currentPath());

    /* in release, when user click icon, do nothing!! */
#ifdef QT_DEBUG
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
#endif

    connect(fswatcher,SIGNAL(fileChanged(QString)),this,SLOT(show()));

    //connect(loginButton, SIGNAL(released()), this, SLOT(login()));
    connect(netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
    //start the web server
    //When this program is shutdown, we want the webserver be shutdown too.
    //so we can't use QProcess::startDetached
    //QProcess::execute("webserver.exe");

    //initData();
    //show();

    //try to take a screenshot every minute
    minuteTimer = new QTimer();
    minuteTimer->setInterval(60000);
    connect(minuteTimer,SIGNAL(timeout()),this,SLOT(shootScreen()));
    minuteTimer->start();
    
    //upload data every hour
    hourTimer = new QTimer();
    hourTimer->setInterval(3600000);
    connect(hourTimer,SIGNAL(timeout()),this,SLOT(sendData()));
    hourTimer->start();
    
    notActive = false;
    HWND thisHwnd = (HWND)this->winId();
    WTSRegisterSessionNotification(thisHwnd, NOTIFY_FOR_ALL_SESSIONS);
}
//! [0]

void TimePie::initData()
{
    //connect to sqlite db "timepie.db" in the installation directory
	//create table `entry` if not already exists
    db = QSqlDatabase::addDatabase("QSQLITE");
    //will be %appdata%\TimePie
    //%appdata% will be mostly likely be roaming
    appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    
    QDir dir(appDataDir);
    if(!dir.exists()){
        dir.mkpath(".");
    }
    
    configFilePath = QString(appDataDir);
    configFilePath.append(QDir::separator()).append("config.json");
    
    QFile configFile;
    configFile.setFileName(configFilePath);
    if(configFile.exists()){
        configFile.open(QIODevice::ReadOnly);
        QString val = configFile.readAll();
        QJsonDocument configJson = QJsonDocument::fromJson(val.toUtf8());
        QJsonObject configObj = configJson.object();
        lastUploadTime = configObj["lastUploadTime"].toInt();
        userName = configObj["userName"].toString();
        userHash = configObj["userHash"].toString();
        userId = configObj["userId"].toString();        
        userStatus = configObj["userStatus"].toString();
    }else{
        configFile.open(QIODevice::WriteOnly);
        QTextStream outStream(&configFile);
        lastUploadTime = 0;
        userName = "";
        userHash = "";
        userId = "";
        userStatus = "";
        outStream << "{\"lastUploadTime\":0,\"userName\":\"\",\"userId\":\"\",\"userHash\":\"\",\"userStatus\":\"\"}";
    }
    configFile.close();
    
    if(userHash != "" && userId != ""){
        reLogin();
    }
    
    QString path(appDataDir);
    path.append(QDir::separator()).append("timepie.db");
    path = QDir::toNativeSeparators(path);
    db.setDatabaseName(path);
	if(db.open()==false){
		QMessageBox::critical(0, QObject::tr("Systray"),
                              QObject::tr("Could not open database"));
	}else{
		QString sqlstr = QString("CREATE TABLE IF NOT EXISTS entry(timestamp integer, name text, title text, forget integer)");
		QSqlQuery query = QSqlQuery(sqlstr,db);
		query.exec();
	}
	qDebug()<< db.tables();

    appDataDir.append(QDir::separator()).append("data");
    dir = QDir(appDataDir);
    if(!dir.exists()){
        dir.mkpath(".");
    }
    
    
}

//! [2]
void TimePie::closeEvent(QCloseEvent *event)
{
    if (trayIcon->isVisible()) {
        /*
        QMessageBox::information(this, tr("Systray"),
                                 tr("The program will keep running in the "
                                    "system tray. To terminate the program, "
                                    "choose <b>Quit</b> in the context menu "
                                    "of the system tray entry."));
        */
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
    //QAction *showMyReport = trayIconMenu->addAction(QString::fromLocal8Bit("My report"));
    //QAction *showSettings = trayIconMenu->addAction(QString::fromLocal8Bit("setting"));
    QAction *showHelp = trayIconMenu->addAction(QString::fromLocal8Bit("Help"));
    QAction *quitAction = trayIconMenu->addAction(QByteArray::fromHex("e98080e587ba").data());

    //connect(showMyReport, SIGNAL(triggered()), this, SLOT(openWebBrowser()));
    //connect(showSettings, SIGNAL(triggered()), this, SLOT(shootScreen()));
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

void TimePie::login(){
    QUrl url = QString("http://angzhou.com/tp/login");
    QNetworkRequest req(url);
    
    QJsonDocument json;
    QJsonObject data;

    data["username"] = nameInput->text();
    data["password"] = pwInput->text();

    json.setObject(data);
    QByteArray jsonPost = QJsonDocument(data).toJson();

    req.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json; charset=utf-8"));
    //req.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(jsonPost.size()));

//Sending the Request
    netManager->post(req, jsonPost);
}

void TimePie::reLogin(){
    QUrl url = QString("http://angzhou.com/tp/relogin");
    QNetworkRequest req(url);
    
    QJsonDocument json;
    QJsonObject data;

    data["userhash"] = userHash;
    data["userid"] = userId;

    json.setObject(data);
    QByteArray jsonPost = QJsonDocument(data).toJson();

    req.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json; charset=utf-8"));

    netManager->post(req, jsonPost);
}

void TimePie::replyFinished(QNetworkReply *serverReply)
{
    if(serverReply->error() != QNetworkReply::NoError)
    {
        statusLabel->setText(QString::fromLocal8Bit("network error"));
        return;
    }

    QByteArray responseData = serverReply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(responseData);
    QJsonObject jsonObj = json.object();
    QString replyType = jsonObj["type"].toString();
    if(replyType == "login" || replyType == "relogin"){
        lastUploadTime = jsonObj["lastUploadTime"].toInt();
        userName = jsonObj["userName"].toString();
        userHash = jsonObj["userHash"].toString();
        userId = jsonObj["userId"].toString();
        userStatus = jsonObj["userStatus"].toString();
        QFile configFile;
        configFile.setFileName(configFilePath);
        configFile.open(QIODevice::WriteOnly);
        configFile.write(json.toJson());
        configFile.close();
        
        loginWidget->hide();
        midWidget->show();
    }else if(replyType == "relogin"){
        loginWidget->hide();
        midWidget->show();        
    }else if(replyType == "upload"){
        //QString fileName = QString("%1\\%2.jpg").arg(appDataDir).arg(jsonObj["file"].toString());
        //if(QFileInfo::exists(fileName)){
        //    QFile::remove(fileName);
        //}
    }
    serverReply->deleteLater();
}

void TimePie::openWebBrowser()
{
    QDesktopServices::openUrl(QUrl("https://angzhou.com/naerqule", QUrl::StrictMode));
}

void TimePie::openHelpBrowser()
{
    QDesktopServices::openUrl(QUrl("http://www.yahoo.com/",QUrl::StrictMode));
}

void TimePie::shootScreen()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString currentApplicationName=QString();
    QString currentWindowTitle=QString();
	int newId;

    //see: http://doc.qt.nokia.com/4.7/desktop-screenshot-screenshot-cpp.html
    QPixmap originalPixmap = QPixmap(); // clear image for low memory situations on embedded devices.

#ifdef Q_OS_WIN
    HWND iHwndForegroundWindow = GetForegroundWindow();

    if(!iHwndForegroundWindow){
        //this will shoot whole desktop,
        //if multi-headed, will shoot the desktop that has foreground window
        originalPixmap = QPixmap::grabWindow(QApplication::desktop()->winId());
        currentApplicationName = QString("None");
        currentWindowTitle = QString();
    }else{
        //Get active window title,
        //if active window is a browser, it will be html doc title
        LPTSTR appTitle = new TCHAR[1023];
        GetWindowText(iHwndForegroundWindow,appTitle,1024);
        if(appTitle!=NULL){
            currentWindowTitle = QString::fromUtf16( (ushort*)appTitle );
		}
		qDebug()<<"title:"<<currentWindowTitle.toUtf8().data();

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
		qDebug()<<"name:"<<currentApplicationName.toUtf8().data();

        if(currentApplicationName!=lastApplicationName || currentWindowTitle!=lastWindowTitle){
            if(db.isOpen()){
				QString sqlstr = QString("insert into entry values('%1','%2','%3',%4)")
                                 .arg(currentTime.toTime_t())
                                 .arg(currentApplicationName)
                                 .arg(currentWindowTitle)
                                 .arg(1);
				qDebug()<<sqlstr.toUtf8().data();

				QSqlQuery query = QSqlQuery(db);
				bool ret = query.exec(sqlstr);
				//qDebug()<<query.lastError().text();

                if (ret){
                    newId = query.lastInsertId().toInt();
				}else{
					qDebug()<<"insert failed\n";
				}
            }

            LPRECT lpRect=new RECT();
            int x, y, w, h;
            if(GetWindowRect(iHwndForegroundWindow,lpRect)){
                x = lpRect->left;
                y = lpRect->top;
                w = lpRect->right - lpRect->left + 1;
                h = lpRect->bottom - lpRect->top + 1;
                originalPixmap = QPixmap::grabWindow(QApplication::desktop()->winId(),
                                                 x,y,w,h);
            }else{
                //This works, but it only grab the pane, not window,
                //i.e. no outer frame, not menu, status.
                originalPixmap = QPixmap::grabWindow((WId)iHwndForegroundWindow);
            }
        }
    }
#else

	//These code actually works on windows too
    WId iHwndForegroundWindow = 0;

    if(!QApplication::activeWindow()){
        iHwndForegroundWindow = QApplication::activeWindow()->winId();
    }

    if(!iHwndForegroundWindow){
        originalPixmap = QPixmap::grabWindow(QApplication::desktop()->winId());
    }else{
        originalPixmap = QPixmap::grabWindow(iHwndForegroundWindow);
        //the diff between activeWindow and focusWidget:
        //There might be an activeWindow() even if there is no focusWidget(),
        //for example if no widget in that window accepts key events.
    }
#endif


	//todo: should we use jpg?
    QString format = "jpg"; //png, jpg, bmp, can not do gif write
	int img_quality = 5;  //default -1, 0:small compressed, 100:large uncompressed

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
    QString fileName = QString("%1/%2.%3").arg(appDataDir).arg(currentTime.toTime_t()).arg(format);
    fileName = QDir::toNativeSeparators(fileName);

	//todo: sometimes have null pixmap, investigate
    if (!fileName.isEmpty() && !originalPixmap.isNull()){
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
    if(userName == "" || userId == "" || userHash == "" || userStatus != "ok"){
        return;
    }

    if(lastUploadTime){
        QUrl url("http://angzhou.com/tp/data/" + userId);
        QNetworkRequest req(url);
        
        QString sqlstr = QString("SELECT timestamp, name, title FROM entry where timestamp >%1").arg(lastUploadTime);
        QSqlQuery query = QSqlQuery(sqlstr, db);
        query.exec();
        QJsonArray jsonArray;
        while (query.next()){
            QJsonObject jsonObj;
            jsonObj.insert("time", query.value(0).toInt());
            jsonObj.insert("name", query.value(1).toString());
            jsonObj.insert("title", query.value(2).toString());
            jsonArray.append(jsonObj);
        }
        QJsonObject data;
        data["data"] = jsonArray;
        QJsonDocument json;
        json.setObject(data);
    
        req.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json; charset=utf-8"));
    
    //Sending the Request
        netManager->post(req, json.toJson());
    }
    QDir dir(appDataDir);
    dir.setFilter(QDir::Files|QDir::NoSymLinks);
    const QFileInfoList fileInfoList = dir.entryInfoList();
    foreach(const QFileInfo& fi, fileInfoList){
        if((unsigned)fi.baseName().toInt() < lastUploadTime){
            QFile::remove(fi.absoluteFilePath());
        }else{
            uploadFile(fi);
        }
    }
    lastUploadTime = QDateTime::currentDateTime().toTime_t();
}

void TimePie::uploadFile(const QFileInfo fi){
    QUrl testUrl("http://angzhou.com/tp/upload/" + userId);
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

/*
void TimepieLoggingHandler(QtMsgType type, const QMessageLogContext &, const QString & str) {
    const char *msg = str.toStdString().c_str();
    switch (type) {
    case QtDebugMsg:
        timepie_logfile << QTime::currentTime().toString().toStdString() << " Debug: " << msg << "\n";
        break;
    case QtCriticalMsg:
        timepie_logfile << QTime::currentTime().toString().toStdString() << " Critical: " << msg << "\n";
        break;
    case QtWarningMsg:
        timepie_logfile << QTime::currentTime().toString().toStdString() << " Warning: " << msg << "\n";
        break;
    case QtFatalMsg:
        timepie_logfile << QTime::currentTime().toString().toStdString() <<  " Fatal: " << msg << "\n";
        abort();
    }
}*/

void TimepieLoggingHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    switch (type) {
    case QtInfoMsg:
        timepie_logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Info: " << msg.toLocal8Bit().constData() << "| in:" << context.file << context.line << context.function << "\n";
        break;
    case QtDebugMsg:
        timepie_logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Debug: " << msg.toLocal8Bit().constData() << "| in:" << context.file << " line:" << context.line << " func:" << context.function << "\n";
        break;
    case QtCriticalMsg:
        timepie_logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Critical: " << context.file << context.line << context.function << "\n";
        break;
    case QtWarningMsg:
        timepie_logfile << QTime::currentTime().toString().toLocal8Bit().constData() << " Warning: " << context.file << context.line << context.function << "\n";
        break;
    case QtFatalMsg:
        timepie_logfile << QTime::currentTime().toString().toLocal8Bit().constData() <<  " Fatal: " << msg.toLocal8Bit().constData() << "\n";
        abort();
     }
    timepie_logfile.flush();
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
    case WM_HOTKEY:
        if(msg->wParam == 100){
            qDebug() << "HotKey worked!!";
            //QProcess::startDetached("G:\\tmp\\sync2.exe");
            //        return true;
        }
        break;
    default:
        break;
    }
    return false;
}

/*
bool TimePie::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type()==QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if ( (key->key()==Qt::Key_Enter) || (key->key()==Qt::Key_Return) ) {
            qDebug() << "got it";
        } else {
            return QObject::eventFilter(obj, event);
        }
        return true;
    } else {
        return QObject::eventFilter(obj, event);
    }
    return false;
}*/

void TimePie::on_confirmButton_clicked()
{
    qDebug() << "confirm clicked";
}
