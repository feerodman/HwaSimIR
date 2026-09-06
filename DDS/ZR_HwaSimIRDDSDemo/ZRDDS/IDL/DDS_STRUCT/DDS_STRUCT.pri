INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/DDS_STRUCT.h \
    $$PWD/DDS_STRUCTDataReader.h \
    $$PWD/DDS_STRUCTDataWriter.h \
    $$PWD/DDS_STRUCTTypeSupport.h \
    $$PWD/servtoproxy_publiser.h \
    $$PWD/servtoproxy_subdcriber.h

SOURCES += \
    $$PWD/DDS_STRUCT.cpp \
    $$PWD/DDS_STRUCTTypeSupport.cpp \
    $$PWD/servtoproxy_publiser.cpp \
    $$PWD/servtoproxy_subdcriber.cpp

DISTFILES += \
    $$PWD/DDS_Struct.idl



























