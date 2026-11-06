QT = core dbus testlib
CONFIG += console c++14 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = dtmftone_test
INCLUDEPATH += ../src
SOURCES += dtmftone_test.cpp
HEADERS += ../src/dtmftone.h
