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

#ifndef COMMHISTORY_QUERY_RESULT_H
#define COMMHISTORY_QUERY_RESULT_H

#include <QHash>
#include <QString>
#include <QPointer>
#include <QSparqlResult>

namespace CommHistory {

class Event;
class Group;
class MessagePart;

typedef enum {
    EventQuery, GroupQuery, MessagePartQuery
} QueryType;

struct QueryResult {
    QString query;
    QueryType queryType;
    Event::PropertySet propertyMask;
    QPointer<QSparqlResult> result;
    // Column mapping by header
    QHash<QString, int> columns;
    // for message part queries
    int eventId;
    QList<Event::Property> properties;

    static void fillEventFromModel(QueryResult &result, Event &event);
    void fillEventFromModel2(Event &event);
    static void fillGroupFromModel(QueryResult &result, Group &group);
    void fillGroupFromModel2(Group &group);
    static void fillMessagePartFromModel(QueryResult &result, MessagePart &part);


    static QString buildContactName(const QString &firstName,
                                    const QString &lastName,
                                    const QString &imNickname);
    static QString buildContactName(const QString &names);
};

} //namespace

#endif // COMMHISTORY_QUERY_RESULT_H
