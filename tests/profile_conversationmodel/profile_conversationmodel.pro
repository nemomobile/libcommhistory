include( ../../common-project-config.pri )
include( ../../common-vars.pri )
include( ../performance_tests.pri )

TARGET = profile_conversationmodel
DESTDIR = ../perf_bin
QT -= gui
SOURCES += conversationmodelprofiletest.cpp
HEADERS += conversationmodelprofiletest.h

