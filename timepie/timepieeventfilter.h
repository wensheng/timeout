#ifndef TIMEPIEEVENTFILTER_H
#define TIMEPIEEVENTFILTER_H

#include <QAbstractNativeEventFilter>
#include <QWidget>
#include <QDebug>
#include "windows.h"

class TimePieEventFilter: public QAbstractNativeEventFilter
{
public:
public:
    void setup(QWidget *target);
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *) override;

private:
    QWidget *target;
};

#endif // TIMEPIEEVENTFILTER_H
