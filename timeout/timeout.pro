#TEMPLATE = app
CONFIG += console qt
QT += core gui widgets network sql

SOURCES = main.cpp \
    timeoutservice.cpp \
    servicemain.cpp \
    timeouteventfilter.cpp

HEADERS += \
    timeoutservice.h \
    servicemain.h \
    timeouteventfilter.h
	
include(qtservice/qtservice.pri)
include(../common/common.pri)
LIBS += -lpsapi
