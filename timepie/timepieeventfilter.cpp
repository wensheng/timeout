#include "timepieeventfilter.h"

void TimePieEventFilter::setup(QWidget *target){
    this->target = target;
}
bool TimePieEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result){
    Q_UNUSED(eventType)
    Q_UNUSED(result)

    // Transform the message pointer to the MSG WinAPI
    MSG* msg = reinterpret_cast<MSG*>(message);
    // If the message is a HotKey, then ...
    if(msg->message == WM_HOTKEY){
        // ... check HotKey
        if(msg->wParam == 100){
            //qDebug() << "HotKey worked";
            target->setWindowState( (target->windowState() & ~Qt::WindowMinimized) |
                                    Qt::WindowActive);
            target->show();
            //target->activateWindow();
            return true;
        }
    }
    return false;
}
