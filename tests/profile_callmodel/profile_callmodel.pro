include( ../../common-project-config.pri )
include( ../../common-vars.pri )
include( ../performance_tests.pri )

TARGET = profile_callmodel
DESTDIR = ../perf_bin
QT -= gui
SOURCES += callmodelprofiletest.cpp
HEADERS += callmodelprofiletest.h

