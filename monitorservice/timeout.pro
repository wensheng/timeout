#TEMPLATE = app
CONFIG += console qt
QT += core widgets

SOURCES = main.cpp \
    timeoutservice.cpp \
    tmmonitor.cpp

HEADERS += \
    timeoutservice.h \
    tmmonitor.h
	
include(qtservice/qtservice.pri)
LIBS += -lpsapi -lwtsapi32 -lAdvapi32
