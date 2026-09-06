INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/HwaSimIRProtocolV1.h \
    $$PWD/HwaSimIRProtocolV1DataReader.h \
    $$PWD/HwaSimIRProtocolV1DataWriter.h \
    $$PWD/HwaSimIRProtocolV1TypeSupport.h \
    $$PWD/hwasimir_publiser.h \
    $$PWD/hwasimir_subscriber.h

SOURCES += \
    $$PWD/HwaSimIRProtocolV1.cpp \
    $$PWD/HwaSimIRProtocolV1TypeSupport.cpp \
    $$PWD/hwasimir_publiser.cpp \
    $$PWD/hwasimir_subscriber.cpp

DISTFILES += $$PWD/HwaSimIRProtocolV1.idl
