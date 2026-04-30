QT += core gui widgets charts

CONFIG += c++17

SOURCES += \
    crate.cpp \
    ltr11.cpp \
    ltr114.cpp \
    ltr212.cpp \
    ltr_result.cpp \
    ltr_workers.cpp \
    main.cpp \
    mainwindow.cpp \
    measurement_writer.cpp \
    sync_timeline.cpp

HEADERS += \
    crate.h \
    ltr11.h \
    ltr114.h \
    ltr212.h \
    ltr_result.h \
    ltr_workers.h \
    mainwindow.h \
    measurement_writer.h \
    module.h \
    sync_timeline.h


FORMS += mainwindow.ui

# Пути к ltrapi.dll и .a (на MinGW)
INCLUDEPATH += $$PWD/LTR

LIBS += $$PWD/LTR/libltrapi.a \
        $$PWD/LTR/libltr11api.a \
        $$PWD/LTR/libltr114api.a \
        $$PWD/LTR/libltr212api.a

#  libltrapi.a называется по прицнипу lib<name>.a (libltrapi.a -> -lltrapi)
