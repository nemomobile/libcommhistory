/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
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
#define COMM_HISTORY_OBJECT_PATH   QLatin1String("/CommHistoryModel")

#define EVENTS_ADDED_SIGNAL        QLatin1String("eventsAdded")
#define EVENTS_UPDATED_SIGNAL      QLatin1String("eventsUpdated")
#define EVENT_DELETED_SIGNAL       QLatin1String("eventDeleted")

#define GROUP_ADDED_SIGNAL         QLatin1String("groupAdded")
#define GROUPS_UPDATED_SIGNAL      QLatin1String("groupsUpdated")
#define GROUPS_UPDATED_FULL_SIGNAL QLatin1String("groupsUpdatedFull")
#define GROUPS_DELETED_SIGNAL      QLatin1String("groupsDeleted")

#define DIVIDER_TODAY              QLatin1String("Today")
#define DIVIDER_YESTERDAY          QLatin1String("Yesterday")
#define DIVIDER_N_WEEKS_AGO        QLatin1String("%1 weeks ago")
#define DIVIDER_N_MONTHS_AGO       QLatin1String("%1 months ago")
#define DIVIDER_OLDER              QLatin1String("Older")


} /* namespace CommHistory */

#endif /* CONSTANTS_H */
