#ifndef SERVICEMAIN_H
#define SERVICEMAIN_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QPixmap>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QUrl>
#include <QDesktopWidget>
#include <QStandardPaths>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QHttpMultiPart>
#include <QtNetwork/QHttpPart>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

#include "windows.h"
#include <tchar.h>
#include <psapi.h> //for GetModuleFileNameEx
#include <WtsApi32.h> // for WTSRegisterSessionNotification

#include <qtservice.h>

class ServiceMain : public QObject
{
    Q_OBJECT
public:
    explicit ServiceMain(QObject *parent = nullptr);

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result);

signals:

public slots:
    void handleTimerEvent();
    void sendData();

private slots:
    void getForegroundProgramInfo();

private:
    void uploadFile(const QFileInfo fi);
    QNetworkAccessManager *netManager;
    QSqlDatabase db;
    QTimer *minuteTimer;
    QTimer *hourTimer;
    QString lastApplicationName;
    QString appDataDir;
    QString lastWindowTitle;
    uint lastUploadTime;
    QString userName;
    QString userId;
    QString userHash;
    QString userStatus;
};

#endif // SERVICEMAIN_H
