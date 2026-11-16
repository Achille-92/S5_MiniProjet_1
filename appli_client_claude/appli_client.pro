QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Interface Qt
SOURCES += \
    main.cpp \
    dialog.cpp \
    radarwidget.cpp \
    udpworker.cpp

HEADERS += \
    dialog.h \
    radarwidget.h \
    scanpoint.h \
    udpworker.h

FORMS += \
    dialog.ui

# Module réseau et protocole, écrits en C (sockets UDP, commandes ANGLE/DIST/SCAN)
SOURCES += \
    udp_client.c \
    radar_proto.c

HEADERS += \
    udp_client.h \
    radar_proto.h

# Winsock (sockets sous Windows)
win32: LIBS += -lws2_32

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
