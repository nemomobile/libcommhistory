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

# -----------------------------------------------------------------------------
# libcommhistory/src/src.pro
# -----------------------------------------------------------------------------
!include( ../common-project-config.pri ) : \
    error( "Unable to include common-project-config.pri" )
!include( ../common-vars.pri ) : \
    error( "Unable to include common-vars.pri" )

# -----------------------------------------------------------------------------
# target setup
# -----------------------------------------------------------------------------
TEMPLATE = lib
VERSION  = $$LIBRARY_VERSION

CONFIG  += shared \
           no_install_prl \
           debug

QT += dbus sql contacts

TARGET = commhistory-qt5
PKGCONFIG += qtcontacts-sqlite-qt5-extensions contactcache-qt5

DEFINES += LIBCOMMHISTORY_SHARED
CONFIG += hide_symbols

# -----------------------------------------------------------------------------
# input
# -----------------------------------------------------------------------------
QT_LIKE_HEADERS += headers/CallEvent \
                   headers/CallModel \
                   headers/ContactListener \
                   headers/ContactResolver \
                   headers/ConversationModel \
                   headers/Event \
                   headers/EventModel \
                   headers/MessagePart \
                   headers/Group \
                   headers/GroupModel \
                   headers/SingleEventModel \
                   headers/RecentContactsModel \
                   headers/Recipient \
                   headers/Events \
                   headers/Models \
                   headers/DatabaseIO

include(sources.pri)

# -----------------------------------------------------------------------------
# Installation target for API header files
# -----------------------------------------------------------------------------
headers.files = $$HEADERS \
                $$QT_LIKE_HEADERS

# -----------------------------------------------------------------------------
# common installation setup
# NOTE: remember to set headers.files before this include to have the headers
# properly setup.
# -----------------------------------------------------------------------------
!include( ../common-installs-config.pri ) : \
    error( "Unable to include common-installs-config.pri" )

# -----------------------------------------------------------------------------
# Installation target for .pc file
# -----------------------------------------------------------------------------
pkgconfig.files = $${TARGET}.pc
pkgconfig.path  = $${INSTALL_PREFIX}/lib/pkgconfig
INSTALLS       += pkgconfig

# -----------------------------------------------------------------------------
# End of file
# -----------------------------------------------------------------------------

OTHER_FILES += \
    sources.pri
