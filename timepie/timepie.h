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

#ifdef _DEBUG
#else
#define QT_NO_DEBUG_OUTPUT
#endif

#include <QApplication>
#include <QtGui>
#include <QStandardPaths>
#include <fstream>
#include <QSystemTrayIcon>
#include <QFileInfo>
#include <QMessageBox>
#include <QDialog>
#include <QtDebug>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QMenu>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>
#include <QTime>
#include <QDateTime>
#include <QProcess>
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
/*class QAction;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QDateTime;
class QMessageBox;*/


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
    void openWebBrowser();
    void openHelpBrowser();
    void shootScreen();
    void sendData();
    void uploadFile(const QFileInfo fi);
    void replyFinished(QNetworkReply *serverReply);
    void login();
    void reLogin();

    void on_confirmButton_clicked();

private:
    void createIconGroupBox();
    void createMessageGroupBox();
    void createTrayIcon();

    Ui::TMController *ui;
    HWND hwndFound;
    QGroupBox *iconGroupBox;
    QLabel *iconLabel;
    QComboBox *iconComboBox;
    QCheckBox *showIconCheckBox;
    QWidget *loginWidget;
    //QWidget *midWidget;
    QLabel *midWidget;
    QNetworkAccessManager *netManager;

    QGroupBox *messageGroupBox;
    QLabel *typeLabel;
    QLineEdit *nameInput;
    QLineEdit *pwInput;
    QLabel *durationLabel;
    QLabel *durationWarningLabel;
    QLabel *statusLabel;
    QLabel *titleLabel;
    QComboBox *typeComboBox;
    QSpinBox *durationSpinBox;
    QLineEdit *titleEdit;
    QPushButton *showMessageButton;

    QAction *minimizeAction;
    QAction *maximizeAction;
    QAction *restoreAction;
    QAction *quitAction;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QFileSystemWatcher *fswatcher;

    QSqlDatabase db;
    QString lastApplicationName;
    QString appDataDir;
    QString lastWindowTitle;
    uint lastUploadTime;
    QString userName;
    QString userId;
    QString userHash;
    QString userStatus;
    QString configFilePath;
    QTimer *minuteTimer;
    QTimer *hourTimer;
    HWND *thisHwnd;
    bool notActive;
};

//! [0]

#endif
