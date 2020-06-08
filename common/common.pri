#include(../common.pri)
INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS += $$PWD/simplecrypt.h \
           $$PWD/odprint.h \
           $$PWD/mustache.h

SOURCES += $$PWD/simplecrypt.cpp \
           $$PWD/mustache.cpp
