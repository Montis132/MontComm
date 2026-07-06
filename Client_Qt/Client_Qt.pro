QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    ClientMsgBussiness.cpp \
    ClientWorker.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    ClientMsgBussiness.h \
    ClientWorker.h \
    mainwindow.h

INCLUDEPATH += \
    ../third_party/libevent/include \
    ../third_party/libevent/build/include \
    ../third_party/json/include \
    ../third_party/msgpack \
    ../third_party/GmSSL/include \
    ../utils/include \
    ../SCShare/include \
    ../Client/src/include

LIBS += \
    -L../third_party/libevent/build/lib -levent \
    -L../third_party/GmSSL/build/bin -lgmssl \
    -L../utils/build/lib -lUtils \
    -L../SCShare/build/lib -lSCShare \
    -lpthread \
    -lcurl \
    -lssl -lcrypto

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    Client_Qt_zh_CN.ts

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    Client_Qt.pro.user
