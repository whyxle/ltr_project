QT += core gui widgets charts

CONFIG += c++17

SOURCES += \
    src/app/main.cpp \
    src/ui/mainwindow.cpp \
    src/ltr/crate.cpp \
    src/ltr/ltr11.cpp \
    src/ltr/ltr114.cpp \
    src/ltr/ltr212.cpp \
    src/ltr/ltr_result.cpp \
    src/ltr/module.cpp \
    src/acquisition/ltr_workers.cpp \
    src/acquisition/sync_timeline.cpp \
    src/io/measurement_writer.cpp

HEADERS += \
    src/ui/mainwindow.h \
    src/ltr/crate.h \
    src/ltr/ltr11.h \
    src/ltr/ltr114.h \
    src/ltr/ltr212.h \
    src/ltr/ltr_result.h \
    src/ltr/module.h \
    src/acquisition/ltr_workers.h \
    src/acquisition/sync_timeline.h \
    src/io/measurement_writer.h


FORMS += src/ui/mainwindow.ui

# Пути к ltrapi.dll и .a (на MinGW)
INCLUDEPATH += $$PWD $$PWD/src $$PWD/LTR

LIBS += $$PWD/LTR/libltrapi.a \
        $$PWD/LTR/libltr11api.a \
        $$PWD/LTR/libltr114api.a \
        $$PWD/LTR/libltr212api.a

#  libltrapi.a называется по прицнипу lib<name>.a (libltrapi.a -> -lltrapi)
