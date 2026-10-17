QT -= core gui
CONFIG += console c++11
TARGET = CustomerTxPayloadProbe
TEMPLATE = app
ZRDDS_ROOT = $$(ZRDDS_MINGW730_ROOT)
isEmpty(ZRDDS_ROOT): error(Set ZRDDS_MINGW730_ROOT)
DEFINES += _ZRDDSCPPINTERFACE _ZRDDSDLLIMPORT
INCLUDEPATH += .. $$ZRDDS_ROOT/include/CPlusPlusInterface $$ZRDDS_ROOT/include/ZRDDSCoreInterface
SOURCES += payload_probe.cpp ../Generated/CustomerTxVideo.cpp ../Generated/CustomerTxVideoTypeSupport.cpp
LIBS += -L$$ZRDDS_ROOT/lib -lZRDDSCpp
