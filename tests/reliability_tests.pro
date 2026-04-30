QT += core testlib

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = reliability_tests

INCLUDEPATH += ..

SOURCES += \
    reliability_tests.cpp \
    ../measurement_writer.cpp \
    ../sync_timeline.cpp

HEADERS += \
    ../measurement_writer.h \
    ../sync_timeline.h
