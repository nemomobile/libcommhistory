/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: John Brooks <john.brooks@jollamobile.com>
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

#ifndef COMMHISTORY_DATABASEIO_P_H
#define COMMHISTORY_DATABASEIO_P_H

#include <QThread>
#include <QSqlDatabase>

#include "event.h"
#include "commonutils.h"

class ContentDeleter;

namespace CommHistory {

class Group;
class DatabaseIO;

/**
 * \class DatabaseIOPrivate
 *
 * Private data and methods for DatabaseIO
 */
class DatabaseIOPrivate : public QObject
{
    Q_OBJECT
    DatabaseIO *q;

public:
    static DatabaseIOPrivate* instance();

    DatabaseIOPrivate(DatabaseIO *parent);
    ~DatabaseIOPrivate();

    static QString makeCallGroupURI(const CommHistory::Event &event);

    static void readEventResult(QSqlQuery &query, Event &event, bool &hasExtraProperties,
            bool &hasMessageParts);
    static void readGroupResult(QSqlQuery &query, Group &group);

    static QString eventQueryBase();

    bool getEvents(const QString &querySuffix, QList<Event> &events);

    ContentDeleter& getContentDeleter(QThread *backgroundThread);

    bool deleteEmptyGroups();

    bool insertEventProperties(int eventId, const QVariantMap &properties);
    bool insertMessageParts(Event &event);

    QSqlQuery createQuery();
    QSqlDatabase& connection();

public:
    QSqlDatabase m_pConnection;

    ContentDeleter *m_ContentDeleter;

    QThread *m_bgThread;
};

} // namespace

#endif
