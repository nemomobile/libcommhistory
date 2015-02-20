/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Slava Monich <slava.monich@jolla.com>
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

#ifndef COMMHISTORY_MMSCONSTANTS_H
#define COMMHISTORY_MMSCONSTANTS_H

// MmsHandler, implemented by commhistory-daemon
#define MMS_HANDLER_BUS         QDBusConnection::systemBus()
#define MMS_HANDLER_SERVICE     "org.nemomobile.MmsHandler"
#define MMS_HANDLER_PATH        "/"
#define MMS_HANDLER_INTERFACE   "org.nemomobile.MmsHandler"

// MmsEngine, implemented by the backend mms-engine
#define MMS_ENGINE_BUS          QDBusConnection::systemBus()
#define MMS_ENGINE_SERVICE      "org.nemomobile.MmsEngine"
#define MMS_ENGINE_PATH         "/"
#define MMS_ENGINE_INTERFACE    "org.nemomobile.MmsEngine"

// Event properties
#define MMS_PROPERTY_IMSI       "mms-notification-imsi"
#define MMS_PROPERTY_PUSH_DATA  "mms-push-data"
#define MMS_PROPERTY_EXPIRY     "mms-expiry"

#endif /* COMMHISTORY_MMSCONSTANTS_H */
