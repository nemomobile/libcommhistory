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

#include "mmsreadreportmodel.h"
#include "mmsconstants.h"
#include "commhistorydatabase.h"
#include "databaseio_p.h"
#include "eventmodel_p.h"

#include <QSqlDatabase>
#include <QSqlQuery>

namespace CommHistory {

class MmsReadReportModel::Private {
public:
    static QSqlQuery buildGroupQuery(int groupId);
};

#define QUERY_EVENT_IS_DRAFT     ":isDraft"
#define QUERY_EVENT_IS_READ      ":isRead"
#define QUERY_EVENT_TYPE         ":type"
#define QUERY_EVENT_DIRECTION    ":direction"
#define QUERY_EVENT_REPORT_READ  ":reportRead"
#define QUERY_EVENT_PROPERTY_KEY ":propertyKey"
#define QUERY_EVENT_GROUP_ID     ":groupId"

QSqlQuery MmsReadReportModel::Private::buildGroupQuery(int groupId)
{
    QString q(DatabaseIOPrivate::eventQueryBase());
    q += " WHERE Events.groupId = " QUERY_EVENT_GROUP_ID
         " AND Events.isDraft = " QUERY_EVENT_IS_DRAFT
         " AND Events.isRead = " QUERY_EVENT_IS_READ
         " AND Events.type = " QUERY_EVENT_TYPE
         " AND Events.direction = " QUERY_EVENT_DIRECTION
         " AND Events.reportRead = " QUERY_EVENT_REPORT_READ
         " AND Events.mmsId != '' "
         " AND Events.id IN ("
         " SELECT DISTINCT eventId from EventProperties"
         " WHERE EventProperties.key = " QUERY_EVENT_PROPERTY_KEY " )"
         " ORDER BY Events.endTime DESC, Events.id DESC";

    QSqlDatabase db(DatabaseIOPrivate::instance()->connection());
    QSqlQuery query = CommHistoryDatabase::prepare(q.toLatin1(), db);
    query.bindValue(QUERY_EVENT_GROUP_ID, groupId);
    query.bindValue(QUERY_EVENT_IS_DRAFT, false);
    query.bindValue(QUERY_EVENT_IS_READ, true);
    query.bindValue(QUERY_EVENT_TYPE, Event::MMSEvent);
    query.bindValue(QUERY_EVENT_DIRECTION, Event::Inbound);
    query.bindValue(QUERY_EVENT_REPORT_READ, true);
    query.bindValue(QUERY_EVENT_PROPERTY_KEY, QString(MMS_PROPERTY_IMSI));
    return query;
}

bool MmsReadReportModel::acceptsEvent(const Event &event)
{
    return (event.type() == Event::MMSEvent &&
            event.direction() == Event::Inbound &&
            event.isRead() &&
            event.reportRead() &&
            !event.mmsId().isEmpty() &&
            !event.extraProperty(MMS_PROPERTY_IMSI).toString().isEmpty());
}

MmsReadReportModel::MmsReadReportModel(QObject *parent) : EventModel(parent), d(NULL)
{
    setQueryMode(EventModel::SyncQuery);
    setResolveContacts(DoNotResolve);
}

MmsReadReportModel::~MmsReadReportModel()
{
}

int MmsReadReportModel::count() const
{
    return EventModel::rowCount();
}

bool MmsReadReportModel::getEvent(int eventId)
{
    if (rowCount() > 0) {
        beginResetModel();
        d_ptr->clearEvents();
        endResetModel();
    }

    if (eventId >= 0) {
        Event event;
        if (d_ptr->database()->getEvent(eventId, event)) {
            QList<Event> events;
            events.append(event);
            return d_ptr->fillModel(0, 1, events, false);
        }
    }

    return false;
}

bool MmsReadReportModel::getEvents(int groupId)
{
    if (rowCount() > 0) {
        beginResetModel();
        d_ptr->clearEvents();
        endResetModel();
    }

    if (groupId >= 0) {
        QSqlQuery query = Private::buildGroupQuery(groupId);
        return d_ptr->executeQuery(query);
    }

    return false;
}

} // CommHistory
