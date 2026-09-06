QT += core gui widgets
CONFIG += c++11
INCLUDEPATH += $$PWD

SOURCES += \
    DdsRuntime.cpp \
    demo.cpp \
    demo2.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    DdsRuntime.h \
    demo.h \
    demo2.h \
    mainwindow.h

FORMS += mainwindow.ui

include($$PWD/ZRDDS/zrdds.prf)
include($$PWD/ZRDDS/IDL/DDS_STRUCT/DDS_STRUCT.pri)
include($$PWD/ZRDDS/IDL/HwaSimIRProtocolV1/HwaSimIRProtocolV1.pri)

DISTFILES += \
    $$PWD/Config/ZRDDS_PROTOCOL_QOS.xml \
    $$PWD/HwaSimIR_DDS_Integration.md