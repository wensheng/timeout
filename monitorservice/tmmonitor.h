#ifndef TMMONITOR_H
#define TMMONITOR_H

#include <QObject>
#include <QProcess>
#include <QDebug>
#include "windows.h"
#include <WtsApi32.h>

class TmMonitor : public QObject
{
    Q_OBJECT
public:
    explicit TmMonitor(QObject *parent = nullptr);

signals:

public slots:

private:
    void invokeTimepie();
    QProcess *proc;
    LPCWSTR timepieProgramPath;
};

#endif // TMMONITOR_H
