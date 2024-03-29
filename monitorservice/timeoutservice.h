#ifndef TIMEOUTSERVICE_H
#define TIMEOUTSERVICE_H

#include <QApplication>
#include <qtservice.h>
#include "tmmonitor.h"

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
    HWND hwndFound;
    TmMonitor *monitor;
};

#endif // TIMEOUTSERVICE_H
