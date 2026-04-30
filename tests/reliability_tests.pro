QT += core testlib

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = reliability_tests

INCLUDEPATH += .. ../src

SOURCES += \
    reliability_tests.cpp \
    ../src/io/measurement_writer.cpp \
    ../src/acquisition/sync_timeline.cpp

HEADERS += \
    ../src/io/measurement_writer.h \
    ../src/acquisition/sync_timeline.h
