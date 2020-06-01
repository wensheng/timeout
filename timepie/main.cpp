/****************************************************************************
**
** Copyright (C) 2011 Wensheng Wang
** All rights reserved.
** Contact: Wensheng Wang (wenshengwang@gmail.com)
**
**
****************************************************************************/

#include <QtGui>
#include "pch.h"
#include "timepie.h"

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(timepie); //initialize stuff in timepie.qrc

    QApplication app(argc, argv);
    app.setOrganizationName(ORGANIZATION_NAME);
    app.setOrganizationDomain(ORGANIZATION_DOMAIN);
    app.setApplicationName(APPLICATION_NAME);
    app.setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(0, QObject::tr("Systray"),
                              QObject::tr("I couldn't detect any system tray "
                                          "on this system."));
        return 1;
    }

    TimePie window;

    //window.show();  //there's only icon, so no window to show

    return app.exec();
}
