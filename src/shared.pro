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

# -----------------------------------------------------------------------------
# libcommhistory/src/shared.pro
# -----------------------------------------------------------------------------
!include( ../common-project-config.pri ) : \
    error( "Unable to include common-project-config.pri" )
!include( ../common-vars.pri ) : \
    error( "Unable to include common-vars.pri" )

# -----------------------------------------------------------------------------
# target setup
# -----------------------------------------------------------------------------
TEMPLATE = lib
TARGET   = commhistory
VERSION  = $$LIBRARY_VERSION

CONFIG  += qdbus \
           shared \
           mobility \
           debug \
           qtsparql
MOBILITY += contacts
DEFINES += LIBCOMMHISTORY_SHARED
QMAKE_CXXFLAGS += -fvisibility=hidden

# -----------------------------------------------------------------------------
# input
# -----------------------------------------------------------------------------
QT_LIKE_HEADERS += headers/CallEvent \
                   headers/CallModel \
                   headers/ConversationModel \
                   headers/DraftModel \
                   headers/Event \
                   headers/EventModel \
                   headers/MessagePart \
                   headers/Group \
                   headers/GroupModel \
                   headers/OutboxModel \
                   headers/SMSInboxModel \
                   headers/SyncSMSModel \
                   headers/UnreadEventsModel \
                   headers/ClassZeroSMSModel \
                   headers/SingleEventModel \
                   headers/Events \
                   headers/Models \
                   headers/TrackerIO

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
pkgconfig.files = commhistory.pc
pkgconfig.path  = $${INSTALL_PREFIX}/lib/pkgconfig
INSTALLS       += pkgconfig

# -----------------------------------------------------------------------------
# End of file
# -----------------------------------------------------------------------------

OTHER_FILES += \
    sources.pri
