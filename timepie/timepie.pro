QT +=  core gui widgets sql network

SOURCES = main.cpp \
	timepie.cpp \
	timepieeventfilter.cpp
	
HEADERS += timepie.h \
	timepieeventfilter.h

RESOURCES = timepie.qrc

include(../common/common.pri)
LIBS += -lpsapi	-lWtsApi32 -lAdvapi32 -luser32 -lNetapi32 -lGdi32 -lGdiplus

FORMS += \
    dialog.ui

