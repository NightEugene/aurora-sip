QT = core network testlib qml
CONFIG += console c++14 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = contactsource_test
INCLUDEPATH += ../src
SOURCES += contactsource_test.cpp ../src/contactsource.cpp ../src/addressbook.cpp
HEADERS += ../src/contactsource.h ../src/addressbook.h
