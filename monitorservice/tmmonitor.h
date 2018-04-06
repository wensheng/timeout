#ifndef TMMONITOR_H
#define TMMONITOR_H

#include <QObject>
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFile>
#include <QTime>
#include <QTimer>
#include <QDebug>
#include <windows.h>
#include <WtsApi32.h>
#include <Psapi.h>

const unsigned int MAX_NAME_SIZE = 512;
const unsigned int TM_INTERVAL = 120000;  // 2 minutes
class TmMonitor : public QObject
{
    Q_OBJECT
public:
    explicit TmMonitor(QObject *parent = nullptr);
    void pauseTimer();

signals:

public slots:

private slots:
    void restartTimePieIfNotRunning();

private:
    void invokeTimepie();
    bool getActiveSessionUserName();
    QString getProcessName(DWORD processId);
    QProcess *proc;
    QString progDataDir;
    wchar_t timepieProgramPath[512];
    QTimer *tmTimer;
    QString activeUserName;
};

#endif // TMMONITOR_H
