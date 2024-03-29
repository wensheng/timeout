#include "timepieeventfilter.h"

void TimePieEventFilter::setup(QDialog *target){
    //this->target = qobject_cast<TimePie *>(target);
    this->target = qobject_cast<TimePie *>(target);
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
            //this is for bring window to front and has focus
            //target->setWindowState( (target->windowState() & ~Qt::WindowMinimized) |
            //                        Qt::WindowActive);
            //target->show();
            //target->activateWindow();
            target->showIfAdmin();
            return true;
        }
    }else if(msg->message == WM_WTSSESSION_CHANGE){
        if(msg->wParam == WTS_SESSION_LOCK){
            target->toggleTimers(true);
        }
        if(msg->wParam == WTS_SESSION_UNLOCK){
            target->toggleTimers(false);
        }
    }
    return false;
}
