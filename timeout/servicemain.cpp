#include "servicemain.h"
#include <fstream>
#include "simplecrypt.h"
using namespace std;

SimpleCrypt crypto(Q_UINT64_C(0xa0f1608c385c24a9));
ServiceMain::ServiceMain(QObject *parent) :
    QObject(parent),
    sharedMemory("TimeoutSharedMemory")
{
    configIsOK = false;
    netManager = new QNetworkAccessManager;
    DWORD pid = GetCurrentProcessId();
    ofstream pidFile;
    //TODO: use applicationdata
    pidFile.open("G:\\tmp\\timeout.pid", ofstream::out);
    pidFile << pid;
    pidFile.close();

    sharedMemory.create(sharedMemorySize);

    /*
       get from timeout.cfg
       this file doesn't exist when Time-Out is first installed and started.
       It is created by tmcontroller.exe, when user entered required information
       after initial installationstartup/reboot, this file should already exists,
       If we can read/decrypt from this file, email and apikey have already been
       verified with the web server.
       If we can't(either it doesn't exist or its content is invalid),
       we keep reading from shared memory, when tmcontroller create this file, it will
       write to shared memory
     */
    if(getConfigFromFile()){
        qDebug() << "timeout.cfg exists and verified";
        configIsOK = true;
        // if verified, set configIsOK to true,
        // config will be written to shared memory
        //verifyWithWebServer();
    }

    connect(netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(processWebServerReply(QNetworkReply*)));
    minuteTimer = new QTimer;
    minuteTimer->setInterval(60000);
    connect(minuteTimer, SIGNAL(timeout()), this, SLOT(getForegroundProgramInfo()));
    minuteTimer->start();

    //upload data every hour
    hourTimer = new QTimer;
    hourTimer->setInterval(3600000);
    connect(hourTimer,SIGNAL(timeout()),this,SLOT(sendData()));
    hourTimer->start();
    qDebug() << "ServieMain initialized!";

}

void ServiceMain::handleTimerEvent()
{
    QtServiceBase::instance()->logMessage(QString("handleTimerEvent"), QtServiceBase::Information );
}

bool ServiceMain::getConfigFromFile(){
    //TODO
    //QString configFilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString configFilePath = "G:\\tmp";
    configFilePath.append(QDir::separator()).append("timeout.cfg");
    QFile configFile(configFilePath);
    if(!configFile.exists()){
        qDebug() << "timeout.cfg doesn't exist.";
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

void ServiceMain::verifyWithWebServer(){
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

void ServiceMain::processWebServerReply(QNetworkReply *serverReply){
    if(serverReply->error() != QNetworkReply::NoError) {
        qDebug() << "network reply error";
        return;
    }

    QByteArray responseData = serverReply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(responseData);
    QJsonObject jsonObj = json.object();
    QString replyType = jsonObj["type"].toString();
    if(replyType == "auth"){
        QString result = jsonObj["result"].toString();
        qDebug() << "Got auth result: " << result;
        if(result == "ok"){
            configIsOK = true;
        }
    }else if(replyType == "relogin"){
        lastUploadTime = jsonObj["lastUploadTime"].toInt();
        userEmail = jsonObj["userName"].toString();
        userApiKey = jsonObj["userHash"].toString();
        userStatus = jsonObj["userStatus"].toString();
        /*
        QFile configFile;
        configFile.setFileName(configFilePath);
        configFile.open(QIODevice::WriteOnly);
        configFile.write(json.toJson());
        configFile.close();
        */
    }else if(replyType == "upload"){
        //QString fileName = QString("%1\\%2.jpg").arg(appDataDir).arg(jsonObj["file"].toString());
        //if(QFileInfo::exists(fileName)){
        //    QFile::remove(fileName);
        //}
    }
    serverReply->deleteLater();
}

/* no longer used, we only read sharedmem here
 * controller write to sharedmem
bool ServiceMain::writeToSharedMemory(){
    if(!sharedMemory.isAttached()){
        sharedMemory.attach();
    }

    QMap<QString,QString> map;
    map.insert("email", userEmail);
    map.insert("apikey", userApiKey);
    map.insert("computer", computerName);
    if(configIsOK){
        map.insert("verified", "true");
    }else{
        map.insert("verified", "false");
    }
    QBuffer buffer;
    buffer.open(QBuffer::WriteOnly);
    QDataStream datastream(&buffer);
    datastream.setVersion(QDataStream::Qt_5_10);
    datastream << map;

    if(buffer.size()>sharedMemorySize){
        qDebug() << "Shared mem too small! This is impossible!";
        return false;
    }
    int size = buffer.size();

    sharedMemory.lock();
    char *writeTo = (char*)sharedMemory.data();
    const char *writeFrom = buffer.data().data();
    memcpy(writeTo, writeFrom, buffer.size());
    sharedMemory.unlock();
    return true;
}
*/

//QMap<QString, QString> ServiceMain::readFromSharedMemory()
QString ServiceMain::readFromSharedMemory(){
    if(!sharedMemory.isAttached()){
        sharedMemory.attach();
    }

    //don't use qmap, just qstring for status
    //QMap<QString,QString> map;
    QString status;
    QBuffer buffer;
    QDataStream datastream(&buffer);
    sharedMemory.lock();
    buffer.setData((char*)sharedMemory.constData(), sharedMemory.size());
    buffer.open(QBuffer::ReadOnly);
    //datastream >> map;
    datastream >> status;
    sharedMemory.unlock();
    //return map;
    return status;
}

bool ServiceMain::getConfigFromSharedMem(){

    return true;
}

void ServiceMain::getForegroundProgramInfo(){
    //QMap<QString,QString> map = readFromSharedMemory();

    if(!configIsOK){
        QString status = readFromSharedMemory();
        if(status.isEmpty()){
            return;
        }else if(status == "ready"){
            sharedMemory.detach();
            qDebug() << "config file is ready, let's read it";
            if(getConfigFromFile()){
                configIsOK = true;
            }else{
                return;
            }
        }
    }
    QDateTime currentTime = QDateTime::currentDateTime();
    QString currentApplicationName=QString();
    QString currentWindowTitle=QString();
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QScreen *screen = QGuiApplication::primaryScreen();
    qDebug() << "screenshoting...";


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
        //qDebug()<<"name:"<<currentApplicationName.toUtf8().data();

        if(currentApplicationName!=lastApplicationName || currentWindowTitle!=lastWindowTitle){
            /*
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
            }*/

            LPRECT lpRect=new RECT();
            int x, y, w, h;
            if(GetWindowRect(iHwndForegroundWindow,lpRect)){
                x = lpRect->left;
                y = lpRect->top;
                w = lpRect->right - lpRect->left + 1;
                h = lpRect->bottom - lpRect->top + 1;
                //originalPixmap = QPixmap::grabWindow(QApplication::desktop()->winId(), x,y,w,h);
                originalPixmap = screen->grabWindow(QApplication::desktop()->winId(), x,y,w,h);
            }else{
                //This works, but it only grab the pane, not window,
                //i.e. no outer frame, not menu, status.
                //originalPixmap = QPixmap::grabWindow((WId)iHwndForegroundWindow);
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
    //QString fileName = QString("%1/%2.%3").arg(appDataDir).arg(currentTime.toTime_t()).arg(format);
    QString fileName = QString("%1.%2").arg(currentTime.toTime_t()).arg(format);
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

void ServiceMain::sendData()
{
    if(!configIsOK){
        return;
    }

    if(lastUploadTime){
        QUrl url("https://ieh.me/a/data/" + userEmail);
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

void ServiceMain::uploadFile(const QFileInfo fi){
    QUrl testUrl("http://angzhou.com/tp/upload/" + userEmail);
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

bool ServiceMain::nativeEvent(const QByteArray &eventType, void *message, long *result){
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
