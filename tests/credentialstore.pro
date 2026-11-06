QT = core testlib
CONFIG += console c++14 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = credentialstore_test
INCLUDEPATH += ../src
SOURCES += credentialstore_test.cpp ../src/credentialstore.cpp
HEADERS += ../src/credentialstore.h
