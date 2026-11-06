QT = core testlib
CONFIG += console c++14 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = sipthread_test
DEFINES += PJ_AUTOCONF=1 PJ_IS_BIG_ENDIAN=0 PJ_IS_LITTLE_ENDIAN=1
INCLUDEPATH += ../src
PJ_TEST_PREFIX = $$(PJ_TEST_PREFIX)
testArchitectures = armv7hl aarch64 x86_64
for(arch, testArchitectures) {
    isEmpty(PJ_TEST_PREFIX):exists($$PWD/../build/$$arch/pj-install/include/pj/config.h): PJ_TEST_PREFIX = $$PWD/../build/$$arch/pj-install
}
isEmpty(PJ_TEST_PREFIX): error(Build an RPM first or set PJ_TEST_PREFIX)
INCLUDEPATH += $$PJ_TEST_PREFIX/include
SOURCES += sipthread_test.cpp ../src/sipthread.cpp
HEADERS += ../src/sipthread.h ../src/reconnectpolicy.h
