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

#ifndef COMMHISTORY_QUERY_RESULT_H
#define COMMHISTORY_QUERY_RESULT_H

#include <QString>
#include <QPointer>
#include <QSparqlQuery>
#include <QSparqlResult>

namespace CommHistory {

class Event;
class Group;
class MessagePart;

typedef enum {
    EventQuery, GroupQuery, MessagePartQuery, GroupedCallQuery, GenericQuery
} QueryType;

struct QueryResult {
    QSparqlQuery query;
    QueryType queryType;
    Event::PropertySet propertyMask;
    QPointer<QSparqlResult> result;
    // for message part queries
    int eventId;
    QList<Event::Property> properties;

    void fillEventFromModel(Event &event);
    void fillGroupFromModel(Group &group);
    void fillMessagePartFromModel(MessagePart &part);
    void fillCallGroupFromModel(Event &event);

    static void parseHeaders(const QString &result,
                             QHash<QString, QString> &headers);

    static void parseContacts(const QString &result, const QString &localUid,
                              QList<Event::Contact> &contacts);

    static QString buildContactName(const QString &firstName,
                                    const QString &lastName,
                                    const QString &contactNickname,
                                    const QString &imNickname);

    // columns for message part query
    enum {
        MessagePartColumnMessage = 0,
        MessagePartColumnMessagePart,
        MessagePartColumnContentId,
        MessagePartColumnText,
        MessagePartColumnMimeType,
        MessagePartColumnCharacterSet,
        MessagePartColumnContentSize,
        MessagePartColumnFileName
    };

    // columns for grouped calls query
    enum {
        CallGroupColumnChannel = 0,
        CallGroupColumnLastCall,
        CallGroupColumnStartTime,
        CallGroupColumnEndTime,
        CallGroupColumnFrom,
        CallGroupColumnTo,
        CallGroupColumnIsSent,
        CallGroupColumnIsAnswered,
        CallGroupColumnIsEmergency,
        CallGroupColumnIsRead,
        CallGroupColumnLastModified,
        CallGroupColumnContacts,
        CallGroupColumnIMNickname,
        CallGroupColumnMissedCount
    };
};

} //namespace

#endif // COMMHISTORY_QUERY_RESULT_H
