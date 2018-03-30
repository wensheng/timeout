#include "servicemain.h"

ServiceMain::ServiceMain(QObject *parent) : QObject(parent)
{
    minuteTimer = new QTimer;
    minuteTimer->setInterval(6000); //should be 60000
    connect(minuteTimer, SIGNAL(timeout()), this, SLOT(getForegroundProgramInfo()));
    minuteTimer->start();

    /*
    //upload data every hour
    QTimer *hourTimer = new QTimer(app);
    hourTimer->setInterval(3600000);
    connect(hourTimer,SIGNAL(timeout()),this,SLOT(sendData()));
    hourTimer->start();*/
    qDebug() << "ServieMain initialized!";

}

void ServiceMain::handleTimerEvent()
{
    QtServiceBase::instance()->logMessage(QString("handleTimerEvent"), QtServiceBase::Information );
}

void ServiceMain::getForegroundProgramInfo(){
    QDateTime currentTime = QDateTime::currentDateTime();
    QString currentApplicationName=QString();
    QString currentWindowTitle=QString();
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QScreen *screen = QGuiApplication::primaryScreen();
    qDebug() << "screenshoting...";


    //see: http://doc.qt.nokia.com/4.7/desktop-screenshot-screenshot-cpp.html
    QPixmap originalPixmap = QPixmap(); // clear image for low memory situations on embedded devices.

    HWND iHwndForegroundWindow = GetForegroundWindow();

    if(iHwndForegroundWindow){
        //Get active window title,
        //if active window is a browser, it will be html doc title
        LPTSTR appTitle = new TCHAR[1023];
        GetWindowText(iHwndForegroundWindow,appTitle,1024);
        if(appTitle!=NULL){
            currentWindowTitle = QString::fromUtf16( (ushort*)appTitle );
        }
        //qDebug()<<"title:"<<currentWindowTitle.toUtf8().data();

        //Get active window application filename
        //e.g. if active window is firefox, it will be "firefox.exe"
        TCHAR *appName = new TCHAR[512];  // for GetModuleFileNameExW
        DWORD procId=0;
        GetWindowThreadProcessId(iHwndForegroundWindow,&procId);

        HANDLE handle = OpenProcess(1040,0,procId);
        //PROCESS_QUERY_INFORMATION(0x400)|PROCESS_VM_READ(0x10)=0x410=1040

        if(handle){
            if(::GetModuleFileNameExW(handle,NULL,appName,512)){
                currentApplicationName = QString::fromUtf16( (ushort*)appName );
            }
        }
        CloseHandle(handle);
        //qDebug()<<"name:"<<currentApplicationName.toUtf8().data();

        if(currentApplicationName!=lastApplicationName || currentWindowTitle!=lastWindowTitle){
            /*
            if(db.isOpen()){
                QString sqlstr = QString("insert into entry values('%1','%2','%3',%4)")
                                 .arg(currentTime.toTime_t())
                                 .arg(currentApplicationName)
                                 .arg(currentWindowTitle)
                                 .arg(1);
                qDebug()<<sqlstr.toUtf8().data();

                QSqlQuery query = QSqlQuery(db);
                bool ret = query.exec(sqlstr);
                //qDebug()<<query.lastError().text();

                if (ret){
                    newId = query.lastInsertId().toInt();
                }else{
                    qDebug()<<"insert failed\n";
                }
            }*/

            LPRECT lpRect=new RECT();
            int x, y, w, h;
            if(GetWindowRect(iHwndForegroundWindow,lpRect)){
                x = lpRect->left;
                y = lpRect->top;
                w = lpRect->right - lpRect->left + 1;
                h = lpRect->bottom - lpRect->top + 1;
                //originalPixmap = QPixmap::grabWindow(QApplication::desktop()->winId(), x,y,w,h);
                originalPixmap = screen->grabWindow(QApplication::desktop()->winId(), x,y,w,h);
            }else{
                //This works, but it only grab the pane, not window,
                //i.e. no outer frame, not menu, status.
                //originalPixmap = QPixmap::grabWindow((WId)iHwndForegroundWindow);
                originalPixmap = screen->grabWindow((WId)iHwndForegroundWindow);
            }
        }
    }


    //todo: should we use jpg?
    QString format = "jpg"; //png, jpg, bmp, can not do gif write
    int img_quality = 25;  //default -1, 0:small compressed, 100:large uncompressed

    /*
    //old debug stuff, don't delete
    QString initialPath = QDir::currentPath() + tr("/untitled.") + format;
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                            initialPath,
                                            tr("%1 Files (*.%2);;All Files (*)")
                                            .arg(format.toUpper())
                                            .arg(format));
    */
    //QString fileName = QString("%1/%2.%3").arg(appDataDir).arg(currentTime.currentMSecsSinceEpoch()).arg(format);
    //QString fileName = QString("%1/%2.%3").arg(appDataDir).arg(currentTime.toTime_t()).arg(format);
    QString fileName = QString("%1.%2").arg(currentTime.toTime_t()).arg(format);
    fileName = QDir::toNativeSeparators(fileName);

    //todo: sometimes have null pixmap, investigate
    if (!fileName.isEmpty() && !originalPixmap.isNull()){
        //save original size
        originalPixmap.save(fileName, format.toStdString().c_str(), img_quality);
        //save half size
        //originalPixmap.scaledToHeight(originalPixmap.height()/2).save(fileName, format.toAscii(), img_quality);
    }

    lastWindowTitle = currentWindowTitle;
    lastApplicationName = currentApplicationName;
}
