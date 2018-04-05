﻿/****************************************************************************
**
** Copyright (C) Wensheng Wang
** All rights reserved.
** Contact: Wensheng Wang (wenshengwang@gmail.com)
**
**
****************************************************************************/

#ifndef TIMEPIE_H
#define TIMEPIE_H

#include <QApplication>
#include <QtGui>
#include <QStandardPaths>
#include <QByteArray>
#include <fstream>
#include <QSystemTrayIcon>
#include <QFileInfo>
#include <QMessageBox>
#include <QDialog>
#include <QtDebug>
#include <QMenu>
#include <QDesktopWidget>
#include <QUrl>
#include <QUrlQuery>
#include <QPixmap>
#include <QTime>
#include <QDateTime>
#include <QHostInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFileSystemWatcher>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "windows.h"
#include <tchar.h>
#include <psapi.h> //for GetModuleFileNameEx
#include <WtsApi32.h> // for WTSRegisterSessionNotification

namespace Ui {
    class TMController;
}

//! [0]
class TimePie : public QDialog
{
    Q_OBJECT

public:
    explicit TimePie(QWidget *parent = 0);

protected:
    //bool eventFilter(QObject *obj, QEvent *event);
    void closeEvent(QCloseEvent *event);
    bool nativeEvent(const QByteArray &eventType, void *message, long *result);

private slots:
    void initData();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void openHelpBrowser();
    void shootScreen();
    void sendData();
    void uploadFile(const QFileInfo fi);
    void processWebServerReply(QNetworkReply *serverReply);

    void on_confirmButton_clicked();
    void on_closeButton_clicked();

private:
    void createIconGroupBox();
    void createMessageGroupBox();
    void createTrayIcon();
    void saveConfigFile();
    bool getConfigFromFile();
    void authWithWebServer();

    Ui::TMController *ui;
    HWND hwndFound;
    QNetworkAccessManager *netManager;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QFileSystemWatcher *fswatcher;

    QSqlDatabase db;
    QString lastApplicationName;
    QString appDataDir;
    QString progDataDir;
    QString lastWindowTitle;
    uint lastUploadTime;
    QString userEmail;
    QString userApiKey;
    QString computerName;
    unsigned int userId;
    int userStatus;
    QString configFilePath;
    QTimer *minuteTimer;
    QTimer *hourTimer;
    HWND *thisHwnd;
    bool notActive;
    bool configIsOK;
};

//! [0]

#endif
