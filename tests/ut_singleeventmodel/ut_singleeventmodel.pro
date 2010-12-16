include( ../../common-project-config.pri )
include( ../../common-vars.pri )
include( ../tests.pri )

TARGET = ut_singleeventmodel
DESTDIR = ../bin
QT += sql
QT -= gui
MOBILITY += contacts
CONFIG  += qtestlib qdbus mobility
SOURCES += singleeventmodeltest.cpp
HEADERS += singleeventmodeltest.h
