/****************************************************************************
**
** Copyright (C) Wensheng Wang
** All rights reserved.
** Contact: Wensheng Wang (wenshengwang@gmail.com)
**
**
****************************************************************************/

#ifndef TIMEPIE_H
#define TIMEPIE_H

#include "QSimpleUpdater.h"
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
#include <QSettings>
#ifdef Q_OS_WIN
#include <wrl.h>
#include <sdkddkver.h>
//#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <windows.h>
#include <tchar.h>
#include <d3d11.h>
#include <dxgi1_2.h>
//#include <dxgi1_5.h>
#endif
#include <lm.h>
namespace Ui {
    class TMController;
}
using namespace Microsoft::WRL;

struct PTSettings{
    bool keepLocalCopies;
    bool isContinuousMode;
    unsigned int continuousModeInterval;
    int dayStartingHour;
    bool noScreenshot;
    QString username;
    QString screenshotSaveDir;
    QString email;
    QString apikey;
    QString computerName;
};

//! [0]
class TimePie : public QDialog
{
    Q_OBJECT

public:
    explicit TimePie(QWidget *parent = 0);
    ~TimePie();
    void showIfAdmin();
    void toggleTimers(bool);

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
    void quit();

    void on_confirmButton_clicked();
    void on_closeButton_clicked();
    void on_screenshotSettingsOKButton_clicked();
    void on_selectLocationPushButton_clicked();
    //void on_keepLocalCheckBox_clicked(bool checked);
    void on_continuousCheckBox_clicked(bool checked);
    void on_intervalMinutesSlider_valueChanged(int value);
    void on_noScreenshotCheckBox_clicked(bool checked);
    void on_openFolderPushButton_clicked();

    void on_keepLocalCheckBox_clicked(bool checked);

    void on_pushButton_clicked();

private:
    void saveSettings();
    void createIconGroupBox();
    void createMessageGroupBox();
    void createTrayIcon();
    void saveConfigFile();
    bool getConfigFromFile();
    void authWithWebServer();
    void captureFullScreenDDA(QString filename);
    bool generateRport(int);
    void openReport(int);

    Ui::TMController *ui;
    HWND hwndFound;
    QNetworkAccessManager *netManager;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QFileSystemWatcher *fswatcher;

    QSqlDatabase db;
    QString sessionUsername;
    QString lastApplicationName;
    QString appDataDir;
    QString progDataDir;
    QString lastWindowTitle;
    uint lastUploadTime;
    ULONGLONG lastScreenShotTime;
    QString userEmail;
    QString userApiKey;
    QString computerName;
    unsigned int userId;
    unsigned int lastScreenshotFileSize;
    int lastInsertTimeStamp;
    int userStatus;
    QString configFilePath;
    QTimer *mainTimer;
    QTimer *dataTimer;
    HWND *thisHwnd;
    bool notActive;
    bool configIsOK;
    bool emailAndKeyValidated;
    bool sessionUserIsAdmin;

    bool ddaInitialized;
    bool frameReleased;
    PTSettings pts;
    uint64_t  lastSumBlue;
    unsigned int lastJpgSize;
    unsigned int sameWinCounter;
    //QSimpleUpdater *m_updater;
};

//! [0]

#endif
