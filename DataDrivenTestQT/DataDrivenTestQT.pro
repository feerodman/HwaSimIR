#-------------------------------------------------
#
# Project created by QtCreator 2026-03-18T14:45:29
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DataDrivenTestQT
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        main.cpp \
        mainwindow.cpp

HEADERS += \
        mainwindow.h

FORMS += \
        mainwindow.ui

DISTFILES += \
        NetworkConfig.ini

CONFIG(debug, debug|release) {
    NETWORK_CONFIG_DEST = $$OUT_PWD/debug/NetworkConfig.ini
} else {
    NETWORK_CONFIG_DEST = $$OUT_PWD/release/NetworkConfig.ini
}

win32:QMAKE_POST_LINK += cmd /c copy /Y $$system_path($$PWD/NetworkConfig.ini) $$system_path($$NETWORK_CONFIG_DEST)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#qnx: icd.path = ./ICD
#!isEmpty(icd.path): INSTALLS += target
