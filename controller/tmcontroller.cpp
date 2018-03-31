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
    ui(new Ui::TMController)
{
    ui->setupUi(this);
    ui->confirmedLabel->hide();
    ui->computernameEdit->setText(QHostInfo::localHostName());
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

    QString svrpidFilePath = QString(appDataDir);
    svrpidFilePath.append(QDir::separator()).append("timeout.pid");
    QFile svrpidFile(svrpidFilePath);
    if(svrpidFile.exists()){
        svrpidFile.open(QIODevice::ReadOnly);
        DWORD svrpid = atol(svrpidFile.readAll());
        qDebug() << svrpid;
        QString svrProcessName = GetProcessName(svrpid);
        if(svrProcessName.isEmpty()){
            ui->isrunningLabel->setText("Time-Out is not running");
            ui->isrunningLabel->setStyleSheet("QLabel { color : red; }");
        }
        //when testing, since this is 32bit program
        // we can only get another 32bit program name
        qDebug() << GetProcessName(svrpid);
    }else{
        ui->isrunningLabel->setText("Time-Out is not running");
    }

    QString configFilePath = QString(appDataDir);
    configFilePath.append(QDir::separator()).append("timeout.cfg");

    QFile configFile;
    configFile.setFileName(configFilePath);
    if(configFile.exists()){
        qDebug() << "exits";
        configFile.open(QIODevice::ReadOnly);
        QString val = configFile.readAll();
        configFile.close();
        qDebug() << val;
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
    QString configFilePath = QString(appDataDir);
    configFilePath.append(QDir::separator()).append("timeout.cfg");
    QFile configFile;
    configFile.setFileName(configFilePath);
    qDebug() << configFilePath;
    userEmail = ui->emailEdit->text();
    userApiKey = ui->apikeyEdit->text();
    computerName = ui->computernameEdit->text();
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

}
