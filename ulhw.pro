TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    netconfapplication.cpp \
    ulakdatastore.cpp

HEADERS += \
    netconfapplication.h \
    utils.h \
    ulakdatastore.h

DISTFILES += \
    netas.yang

LIBS += -lyang -lnetconf2 -lsysrepo

DEFINES += NC_ENABLED_SSH

include (3rdparty/loguru/loguru.pri)
