QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
INCLUDEPATH += $$PWD

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    demo.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    demo.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

include($$PWD/ZRDDS/IDL/DDS_STRUCT/DDS_STRUCT.pri)

include($$PWD/ZRDDS/zrdds.prf)
#include($$PWD/dds/dds.prf)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


#DEFINES += _ZRDDSCPPINTERFACE

#INCLUDEPATH += $$quote($$PWD/dds/include/ZRDDSCoreInterface)
#INCLUDEPATH += $$quote($$PWD/dds/include/CPlusPlusInterface)

#CONFIG(debug, debug|release) {
#     LIBS += -L$$quote($$PWD/dds/lib) -lZRDDSCppzd -lws2_32 -lwsock32 -liphlpapi
#}

#CONFIG(release, debug|release) {
#     LIBS += -L$$quote($$PWD/dds/lib) -lZRDDSCppz -lws2_32 -lwsock32 -liphlpapi
#}


