#include "timeouteventfilter.h"

bool TimeoutEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result){
    Q_UNUSED(eventType)
    Q_UNUSED(result)

    // Transform the message pointer to the MSG WinAPI
    MSG* msg = reinterpret_cast<MSG*>(message);
    // If the message is a HotKey, then ...
    if(msg->message == WM_HOTKEY){
        // ... check HotKey
        if(msg->wParam == 100){
            qDebug() << "HotKey worked";
            QProcess::startDetached("G:\\tmp\\sync2.exe");
            return true;
        }
    }
    return false;
}
