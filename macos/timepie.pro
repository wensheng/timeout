QT +=  core gui widgets sql network macextras

include ($$PWD/../3rdParty/QSimpleUpdater/QSimpleUpdater.pri)

SOURCES += main.mm \
	timepie.mm \
	timepieeventfilter.mm

HEADERS += timepie.h \
	pch.h \
	timepieeventfilter.h

RESOURCES = timepie.qrc

include (../common/common.pri)

LIBS += -framework ApplicationServices

FORMS += \
    dialog.ui

QMAKE_INFO_PLIST = resource/Info.plist

