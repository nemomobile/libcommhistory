include( ../../common-project-config.pri )
include( ../../common-vars.pri )
include( ../tests.pri )

TARGET = ut_eventsquery
DESTDIR = ../bin
MOBILITY += contacts
CONFIG  += qtestlib qdbus mobility
SOURCES += eventsquerytest.cpp
HEADERS += eventsquerytest.h
