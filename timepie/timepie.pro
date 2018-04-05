QT += xml svg sql network

SOURCES = main.cpp \
	timepie.cpp \
	timepieeventfilter.cpp
	
HEADERS += timepie.h \
	timepieeventfilter.h

RESOURCES = timepie.qrc

include(../common/common.pri)
LIBS += -lpsapi	-lWtsApi32 -lAdvapi32 -luser32

# install
target.path = desktop
#sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS timepie.pro resources images
#sources.path = .
INSTALLS += target sources

FORMS += \
    dialog.ui

