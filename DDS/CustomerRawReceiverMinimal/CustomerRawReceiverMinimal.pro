QT -= core gui
CONFIG += console c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = CustomerRawReceiverMinimal
ZRDDS_ROOT = $$(ZRDDS_MINGW730_ROOT)
isEmpty(ZRDDS_ROOT): error(Set ZRDDS_MINGW730_ROOT to the installed matching SDK)
DEFINES += _ZRDDSCPPINTERFACE _ZRDDSDLLIMPORT
INCLUDEPATH += ../CustomerTX/Generated $$ZRDDS_ROOT/include/CPlusPlusInterface $$ZRDDS_ROOT/include/ZRDDSCoreInterface
SOURCES += main.cpp ../CustomerTX/Generated/CustomerTxVideo.cpp ../CustomerTX/Generated/CustomerTxVideoTypeSupport.cpp
LIBS += -L$$ZRDDS_ROOT/lib -lZRDDSCpp
