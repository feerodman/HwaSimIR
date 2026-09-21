QT += core gui widgets network testlib
TEMPLATE = app
TARGET = p15_ui_default_visibility_test
CONFIG += c++11 console
CONFIG -= app_bundle

INCLUDEPATH += .. ../../HwaSim_IR/HwaSim_IR/IR ../../Shared

SOURCES += \
    p15_ui_default_visibility_test.cpp \
    ../mainwindow.cpp \
    ../../HwaSim_IR/HwaSim_IR/IR/IRSolarPosition.cpp \
    ../../HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.cpp

HEADERS += \
    ../mainwindow.h \
    ../ReplayCsvSchema.h \
    ../../Shared/NumericCsvReader.h \
    ../../HwaSim_IR/HwaSim_IR/IR/IRSolarPosition.h \
    ../../HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h \
    ../../HwaSim_IR/HwaSim_IR/IR/IRTypes.h
