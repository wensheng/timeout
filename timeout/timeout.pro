#TEMPLATE = app
CONFIG += console qt
QT += core gui widgets network sql

SOURCES = main.cpp \
    timeoutservice.cpp \
    servicemain.cpp \
    timeouteventfilter.cpp \
	../common/simplecrypt.cpp

include(qtservice/qtservice.pri)

HEADERS += \
    timeoutservice.h \
    servicemain.h \
    timeouteventfilter.h \
	../common/simplecrypt.h

LIBS += -lpsapi
