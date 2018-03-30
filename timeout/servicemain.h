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
#include <QDesktopWidget>
#include <QStandardPaths>
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

signals:

public slots:
    void handleTimerEvent();

private slots:
    void getForegroundProgramInfo();

private:
    QTimer *minuteTimer;
    //QTimer *hourTimer;
    QString lastApplicationName;
    QString appDataDir;
    QString lastWindowTitle;
};

#endif // SERVICEMAIN_H
