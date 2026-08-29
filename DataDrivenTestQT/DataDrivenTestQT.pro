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
        NetworkConfig.ini \
        ../HwaSim_IR/Bin/Config/DDS/ZRDDS_PROTOCOL_QOS.xml

# DataDrivenTestQT is built with MinGW 7.3.0 and must use the matching ZRDDS
# C++ SDK. Never point this target at the VS2015 import library.
ZRDDS_MINGW730_ROOT = $$(ZRDDS_MINGW730_ROOT)
ZRDDS_LICENSE_FILE = $$(ZRDDS_LICENSE_FILE)
isEmpty(ZRDDS_MINGW730_ROOT):exists(F:/Programs/ZRDDS/ZRDDS_MinGW7.3.0/ZRDDS-2.4.5/include/CPlusPlusInterface/ZRDDSCppSimpleInterface.h) {
    ZRDDS_MINGW730_ROOT = F:/Programs/ZRDDS/ZRDDS_MinGW7.3.0/ZRDDS-2.4.5
}
isEmpty(ZRDDS_LICENSE_FILE):exists(F:/Programs/ZRDDS/ZRDDS_VS2015/ZRDDS-2.4.5/zrddslicence.lic) {
    ZRDDS_LICENSE_FILE = F:/Programs/ZRDDS/ZRDDS_VS2015/ZRDDS-2.4.5/zrddslicence.lic
}
!isEmpty(ZRDDS_MINGW730_ROOT) {
    DEFINES += HWASIMIR_HAS_ZRDDS=1 _ZRDDSCPPINTERFACE _ZRDDSDLLIMPORT
    INCLUDEPATH += \
        $$PWD/../DDS/Protocol \
        $$PWD/../DDS/Runtime \
        $$PWD/../DDS/Generated/HwaSimIRProtocolV1 \
        $$ZRDDS_MINGW730_ROOT/include/CPlusPlusInterface \
        $$ZRDDS_MINGW730_ROOT/include/ZRDDSCoreInterface
    SOURCES += \
        $$PWD/../DDS/Protocol/DdsStimClient.cpp \
        $$PWD/../DDS/Protocol/CommonDataDdsAdapter.cpp \
        $$PWD/../DDS/Runtime/DdsRuntimeManager.cpp \
        $$PWD/../DDS/Generated/HwaSimIRProtocolV1/HwaSimIRProtocolV1.cpp \
        $$PWD/../DDS/Generated/HwaSimIRProtocolV1/HwaSimIRProtocolV1TypeSupport.cpp
    HEADERS += $$PWD/../DDS/Protocol/DdsStimClient.h
    LIBS += -L$$ZRDDS_MINGW730_ROOT/lib -lZRDDSCpp
} else {
    warning("MinGW 7.3.0 ZRDDS SDK not found; DDS transport will fail fast at runtime")
}

CONFIG(debug, debug|release) {
    NETWORK_CONFIG_DEST = $$OUT_PWD/debug/NetworkConfig.ini
} else {
    NETWORK_CONFIG_DEST = $$OUT_PWD/release/NetworkConfig.ini
}

win32:QMAKE_POST_LINK += cmd /c copy /Y $$system_path($$PWD/NetworkConfig.ini) $$system_path($$NETWORK_CONFIG_DEST)
win32:QMAKE_POST_LINK += $$escape_expand(\n\t) cmd /c if not exist $$system_path($$OUT_PWD/release/Config/DDS) mkdir $$system_path($$OUT_PWD/release/Config/DDS)
win32:QMAKE_POST_LINK += $$escape_expand(\n\t) cmd /c copy /Y $$system_path($$PWD/../HwaSim_IR/Bin/Config/DDS/ZRDDS_PROTOCOL_QOS.xml) $$system_path($$OUT_PWD/release/Config/DDS/ZRDDS_PROTOCOL_QOS.xml)
win32:!isEmpty(ZRDDS_MINGW730_ROOT):QMAKE_POST_LINK += $$escape_expand(\n\t) cmd /c copy /Y $$system_path($$ZRDDS_MINGW730_ROOT/lib/ZRDDSCpp.dll) $$system_path($$OUT_PWD/release/ZRDDSCpp.dll)
win32:!isEmpty(ZRDDS_LICENSE_FILE):QMAKE_POST_LINK += $$escape_expand(\n\t) cmd /c copy /Y $$system_path($$ZRDDS_LICENSE_FILE) $$system_path($$OUT_PWD/release/zrddslicence.lic)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#qnx: icd.path = ./ICD
#!isEmpty(icd.path): INSTALLS += target
