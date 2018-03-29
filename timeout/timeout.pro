#TEMPLATE = app
CONFIG += console qt
QT += core gui widgets

SOURCES = main.cpp \
    timeoutservice.cpp \
    servicemain.cpp \
    timeouteventfilter.cpp

include(qtservice/qtservice.pri)

HEADERS += \
    timeoutservice.h \
    servicemain.h \
    timeouteventfilter.h

LIBS += -lpsapi
