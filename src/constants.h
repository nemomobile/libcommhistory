/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#ifndef COMMHISTORY_CONSTANTS_H
#define COMMHISTORY_CONSTANTS_H

namespace CommHistory {


#define COMM_HISTORY_SERVICE_NAME  QLatin1String("com.nokia.commhistory")
#define COMM_HISTORY_INTERFACE     QLatin1String("com.nokia.commhistory")
#define COMM_HISTORY_OBJECT_PATH   QLatin1String("/CommHistoryModel")

#define EVENTS_ADDED_SIGNAL        QLatin1String("eventsAdded")
#define EVENTS_UPDATED_SIGNAL      QLatin1String("eventsUpdated")
#define EVENT_DELETED_SIGNAL       QLatin1String("eventDeleted")

#define GROUPS_ADDED_SIGNAL        QLatin1String("groupsAdded")
#define GROUPS_UPDATED_SIGNAL      QLatin1String("groupsUpdated")
#define GROUPS_UPDATED_FULL_SIGNAL QLatin1String("groupsUpdatedFull")
#define GROUPS_DELETED_SIGNAL      QLatin1String("groupsDeleted")

#define EVENT_PROPERTY_SUBSCRIBER_ID    QLatin1String("subscriberIdentity")

} /* namespace CommHistory */

#endif /* CONSTANTS_H */
