#include(../common.pri)
INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
!win32:QT += network
win32:LIBS += -luser32

qtservice-uselib:!qtservice-buildlib {
    LIBS += -L$$QTSERVICE_LIBDIR -l$$QTSERVICE_LIBNAME
} else {
    HEADERS       += $$PWD/qtservice.h \
                     $$PWD/qtservice_p.h
    SOURCES       += $$PWD/qtservice.cpp
    win32:SOURCES += $$PWD/qtservice_win.cpp
}

win32 {
    qtservice-buildlib:shared:DEFINES += QT_QTSERVICE_EXPORT
    else:qtservice-uselib:DEFINES += QT_QTSERVICE_IMPORT
}
