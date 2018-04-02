#ifndef TIMEOUTSERVICE_H
#define TIMEOUTSERVICE_H

#include <QApplication>
#include <QDesktopWidget>
#include <QLabel>
#include <QDir>
#include <QSettings>

#include <qtservice.h>
#include "servicemain.h"
#include "windows.h"

class TimeoutService : public QtService<QApplication>
{
public:
    TimeoutService(int argc, char **argv);
    ~TimeoutService();

protected:

    void start();
    void stop();
    void pause();
    void resume();
    void processCommand(int code);

private:
    QLabel *gui;
    ServiceMain *serviceMain;
    HWND hwndFound;

};

#endif // TIMEOUTSERVICE_H
