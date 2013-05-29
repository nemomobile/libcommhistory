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

include( ../common-project-config.pri )
include( ../common-vars.pri )

TEMPLATE  = lib
CONFIG   += static debug qtsparql
PKGCONFIG += tracker-sparql-0.14
MOBILITY += contacts
VERSION   = $$LIBRARY_VERSION
DEFINES += LIBCOMMHISTORY_SHARED
QMAKE_CXXFLAGS += -fvisibility=hidden

QT += dbus

equals(QT_MAJOR_VERSION, 4) {
    TARGET = commhistory
    CONFIG += mobility
    MOBILITY += contacts
}

equals(QT_MAJOR_VERSION, 5) {
    TARGET = commhistory-qt5
    QT += contacts
}

include(sources.pri)

include( ../common-installs-config.pri )
