TEMPLATE = app
TARGET = ru.nighteugene.aurorasip
VERSION = 1.0.0
QT += qml quick multimedia network dbus
CONFIG += c++14 auroraapp link_pkgconfig
DEFINES += PJ_AUTOCONF=1 PJ_IS_BIG_ENDIAN=0 PJ_IS_LITTLE_ENDIAN=1
INCLUDEPATH += $$(PBX_PREFIX)/include
LIBS += $$system(PKG_CONFIG_SYSROOT_DIR= pkg-config --libs --static libpjproject)
PKGCONFIG += ngf-qt5 callservice auroraapp qca2-qt5
SOURCES += src/credentialworker.cpp src/keypadtone.cpp src/sipthread.cpp src/contactsource.cpp src/addressbook.cpp src/main.cpp src/phone.cpp src/credentialstore.cpp
HEADERS += src/credentialpayload.h src/accountsettings.h src/shutdownguard.h src/credentialworker.h src/keypadtone.h src/dtmftone.h src/reconnectpolicy.h src/sipthread.h src/contactsource.h src/addressbook.h src/appsettings.h src/phone.h src/credentialstore.h
AURORAAPP_ICONS = 86x86 108x108 128x128 172x172

# Explicit opt-in for device automation; never exported by ordinary builds.
contains(CONFIG, aurorasip_diagnostics) {
    DEFINES += AURORA_SIP_DIAGNOSTICS
    HEADERS += src/accountipc.h
}
