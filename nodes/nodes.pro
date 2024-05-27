#-------------------------------------------------
#
# Project created by QtCreator 2017-12-04T17:36:37
#
#-------------------------------------------------

QT += core gui widgets serialport dbus

TARGET = nodes
TEMPLATE = app


SOURCES += \
    main.cpp \
    dialog.cpp \
    frame.cpp \
    matrix.cpp

PRECOMPILED_HEADER += \
    precompiled.h

HEADERS += \
    precompiled.h \
    dialog.h \
    frame.h \
    matrix.h \

FORMS += \
    dialog.ui \
    frame.ui \
    matrix.ui

RESOURCES += \
    nodes.qrc
