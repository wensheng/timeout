QT +=  core gui widgets sql network

DEFINES += NOMINMAX

SOURCES = main.cpp \
	timepie.cpp \
	timepieeventfilter.cpp
	
HEADERS += timepie.h \
	pch.h \
	timepieeventfilter.h

RESOURCES = timepie.qrc

include(../common/common.pri)
INCLUDEPATH += DirectXTK/Inc
LIBS += -lpsapi	-lWtsApi32 -lAdvapi32 -luser32 -lNetapi32 -lGdi32 -lGdiplus -lruntimeobject -L$$PWD/DirectXTK/Bin/Desktop_2019_Win10/x64/Debug -lDirectXTK

FORMS += \
    dialog.ui

