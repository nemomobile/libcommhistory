###############################################################################
#
# This file is part of libcommhistory.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
###############################################################################

include( ../common-project-config.pri )
include( ../common-vars.pri )

TEMPLATE  = lib
QT       += sql
CONFIG   += qdbus static mobility debug
MOBILITY += contacts
VERSION   = $$LIBRARY_VERSION
TARGET    = commhistory
DEFINES += LIBCOMMHISTORY_SHARED
QMAKE_CXXFLAGS += -fvisibility=hidden

HEADERS += trackerio.h \
           commonutils.h \
           eventmodel.h \
           queryrunner.h \
           eventmodel_p.h \
           event.h \
           messagepart.h \
           callevent.h \
           eventtreeitem.h \
           conversationmodel.h \
           callmodel.h \
           callmodel_p.h \
           draftmodel.h \
           smsinboxmodel.h \
           syncsmsmodel.h \
           outboxmodel.h \
           groupmodel.h \
           groupmodel_p.h \
           group.h \
           adaptor.h \
           unreadeventsmodel.h \
           conversationmodel_p.h \
           classzerosmsmodel.h \
           mmscontentdeleter.h \
           updatequery.h \
           contactlistener.h \
           libcommhistoryexport.h \
           idsource.h \
           trackerio_p.h \
           queryresult.h \
           singleeventmodel.h \
           committingtransaction.h

SOURCES += trackerio.cpp \
           commonutils.cpp \
           eventmodel.cpp \
           queryrunner.cpp \
           eventmodel_p.cpp \
           eventtreeitem.cpp \
           conversationmodel.cpp \
           callmodel.cpp \
           draftmodel.cpp \
           smsinboxmodel.cpp \
           syncsmsmodel.cpp \
           outboxmodel.cpp \
           groupmodel.cpp \
           group.cpp \
           adaptor.cpp \
           event.cpp \
           messagepart.cpp \
           unreadeventsmodel.cpp \
           classzerosmsmodel.cpp \
           mmscontentdeleter.cpp \
           contactlistener.cpp \
           idsource.cpp \
           queryresult.cpp \
           singleeventmodel.cpp

include( ../common-installs-config.pri )
