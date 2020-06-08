#ifndef TIMEPIEEVENTFILTER_H
#define TIMEPIEEVENTFILTER_H

#include <QAbstractNativeEventFilter>
#include <QWidget>
#include <QDebug>
#include "timepie.h"
//#include <Windows.h>
#include "windows.h"

class TimePieEventFilter: public QAbstractNativeEventFilter
{
public:
public:
    void setup(QDialog *target);
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *) override;

private:
    TimePie *target;
};

#endif // TIMEPIEEVENTFILTER_H
