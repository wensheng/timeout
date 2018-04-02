#include "tmcontroller.h"
#include "ui_tmcontroller.h"
#include "simplecrypt.h"
#include <QApplication>
#include <fstream>
#include <stdlib.h>

std::ofstream timepie_logfile;
void TimepieLoggingHandler(QtMsgType type, const QMessageLogContext &, const QString &msg);
QString GetProcessName( DWORD processID );
SimpleCrypt crypto(Q_UINT64_C(0xa0f1608c385c24a9));

TMController::TMController(QWidget *parent) :
    QDialog(parent),
    sharedMemory("TimeoutSharedMemory"),
    ui(new Ui::TMController)
{
    ui->setupUi(this);
    ui->confirmedLabel->hide();
    ui->computernameEdit->setText(QHostInfo::localHostName());
    netManager = new QNetworkAccessManager;
    connect(netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(processWebServerReply(QNetworkReply*)));
    sharedMemory.create(sharedMemorySize);
    initData();
}

TMController::~TMController()
{
    delete ui;
}

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
}

void TMController::initData()
{
    //%appdata% will be mostly likely be roaming
    appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    appDataDir = "\\tmp";

    QDir dir(appDataDir);
    if(!dir.exists()){
        dir.mkpath(".");
    }

    /*
     * after installation, timeout service is run, timeout.pid is created
     * if service stop, GetProcessName will return ""
     *
     **/
    serviceIsRunning = false;
    QString svrpidFilePath = QString(appDataDir);
    svrpidFilePath.append(QDir::separator()).append("timeout.pid");
    QFile svrpidFile(svrpidFilePath);
    if(svrpidFile.exists()){
        svrpidFile.open(QIODevice::ReadOnly);
        DWORD svrpid = atol(svrpidFile.readAll());
        qDebug() << svrpid;
        //when testing, since this is 32bit program
        // we can only get another 32bit program name
        QString svrProcessName = GetProcessName(svrpid);
        if(!svrProcessName.isEmpty()){
            serviceIsRunning = true;
        }
        qDebug() << svrProcessName;
    }

    if(serviceIsRunning){
        ui->isrunningLabel->setText("Time-Out is running");
        ui->isrunningLabel->setStyleSheet("QLabel { color : green; }");
    }else{
        ui->isrunningLabel->setText("Time-Out is not running");
        ui->isrunningLabel->setStyleSheet("QLabel { color : red; }");
    }

    QString configFilePath = QString(appDataDir);
    configFilePath.append(QDir::separator()).append("timeout.cfg");

    QFile configFile;
    configFile.setFileName(configFilePath);
    if(configFile.exists()){
        qDebug() << "config file exists";
        configFile.open(QIODevice::ReadOnly);
        QString val = configFile.readAll();
        configFile.close();
        QByteArray decrypted = crypto.decryptToByteArray(val);
        if(crypto.lastError()){
            return;
        }
        QJsonDocument configJson = QJsonDocument::fromJson(decrypted);
        QJsonObject configObj = configJson.object();
        userEmail = configObj["Email"].toString();
        ui->emailEdit->setText(userEmail);
        userApiKey = configObj["ApiKey"].toString();
        ui->apikeyEdit->setText(userApiKey);
        //userStatus = configObj["userStatus"].toString();
        computerName = configObj["Computer"].toString();
        ui->computernameEdit->setText(computerName);
    }/*
    else{
        configFile.open(QIODevice::WriteOnly);
        QTextStream outStream(&configFile);
        userEmail = "";
        userApiKey = "";
        userStatus = "";
        outStream << "{\"lastUploadTime\":0,\"userName\":\"\",\"userId\":\"\",\"userHash\":\"\",\"userStatus\":\"\"}";
    }*/

}

void TMController::on_confirmButton_clicked()
{

    userEmail = ui->emailEdit->text();
    userApiKey = ui->apikeyEdit->text();
    computerName = ui->computernameEdit->text();

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

void TMController::processWebServerReply(QNetworkReply *serverReply){
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
            saveConfigFile();
            //writeToSharedMemory();
            //configIsOK = true;
        }
    }
    //writeToSharedMemory();
    serverReply->deleteLater();
}

void TMController::saveConfigFile(){
    QString configFilePath = QString(appDataDir);
    configFilePath.append(QDir::separator()).append("timeout.cfg");
    QFile configFile(configFilePath);
    qDebug() << configFilePath;
    QJsonObject jsonObj;
    jsonObj.insert("Email", QJsonValue::fromVariant(userEmail));
    jsonObj.insert("ApiKey", QJsonValue::fromVariant(userApiKey));
    jsonObj.insert("Computer", QJsonValue::fromVariant(computerName));
    QJsonDocument json(jsonObj);
    configFile.open(QIODevice::WriteOnly);
    QTextStream outStream(&configFile);
    outStream << crypto.encryptToString(json.toJson());
    configFile.close();
    ui->confirmedLabel->show();

    if(!sharedMemory.isAttached()){
        sharedMemory.attach();
    }

    QString status = "ready";
    QBuffer buffer;
    buffer.open(QBuffer::WriteOnly);
    QDataStream datastream(&buffer);
    datastream.setVersion(QDataStream::Qt_5_10);
    datastream << status;

    sharedMemory.lock();
    char *writeTo = (char*)sharedMemory.data();
    const char *writeFrom = buffer.data().data();
    memcpy(writeTo, writeFrom, buffer.size());
    sharedMemory.unlock();
}

void TMController::on_closeButton_clicked()
{
    QApplication::quit();

}

QString GetProcessName( DWORD processID ){
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

    // Release the handle to the process.
    CloseHandle( hProcess );
    return QString::fromWCharArray(szProcessName);
}

void TMController::on_runButton_clicked()
{
    ui->isrunningLabel->setText("Time-Out is running.");
    ui->isrunningLabel->setStyleSheet("QLabel { color : green; }");
    ui->runButton->hide();

}
