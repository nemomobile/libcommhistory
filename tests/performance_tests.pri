###############################################################################
#
# This file is part of libcommhistory.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Reto Zingg <reto.zingg@nokia.com>
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

QT          += testlib dbus
TEMPLATE     = app
INCLUDEPATH += . ../../src ..
DEPENDPATH  += $${INCLUDEPATH}

LIBS += ../../src/libcommhistory-qt5.so
QT += contacts
PKGCONFIG += qtcontacts-sqlite-qt5-extensions contactcache-qt5

SOURCES += ../common.cpp
HEADERS += ../common.h

DEFINES += PERF_ITERATIONS=5
DEFINES += PERF_BATCH_SIZE=25

!include( ../common-installs-config.pri ) : \
    error( "Unable to include common-installs-config.pri!" )
# override default path for tests
target.path = /opt/tests/$${PROJECT_NAME}-performance-tests
# End of File

