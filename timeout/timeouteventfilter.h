#ifndef TIMEOUTEVENTFILTER_H
#define TIMEOUTEVENTFILTER_H

#include <QAbstractNativeEventFilter>
#include <QDebug>
#include "windows.h"

class TimeoutEventFilter: public QAbstractNativeEventFilter
{
public:
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *) override;
};

#endif // TIMEOUTEVENTFILTER_H
