/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014-2015 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@jolla.com>
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

#include <QtDBus/QtDBus>
#include <QSqlQuery>

#include "eventmodel_p.h"
#include "conversationmodel.h"
#include "conversationmodel_p.h"
#include "constants.h"
#include "mmsconstants.h"
#include "commhistorydatabase.h"
#include "databaseio_p.h"
#include "debug.h"

namespace {
static CommHistory::Event::PropertySet unusedProperties = CommHistory::Event::PropertySet()
                                             << CommHistory::Event::IsDraft
                                             << CommHistory::Event::IsMissedCall
                                             << CommHistory::Event::IsEmergencyCall
                                             << CommHistory::Event::BytesReceived
                                             << CommHistory::Event::EventCount;
}
namespace CommHistory {

// Filters
class ConversationModelPrivate::Filter {
public:
    virtual ~Filter() {}
    virtual QString filterString() const = 0;
    virtual void bindValues(QSqlQuery &query) const = 0;
    virtual bool acceptsEvent(const Event &event) const = 0;
};

class ConversationModelPrivate::FilterDefault: public ConversationModelPrivate::Filter {
public:
    FilterDefault(Event::EventType type, const QString &account, Event::EventDirection direction) :
        filterType(type), filterAccount(account), filterDirection(direction) {}
    ~FilterDefault() {}

    QString filterString() const;
    void bindValues(QSqlQuery &query) const;
    bool acceptsEvent(const Event &event) const;

private:
    Event::EventType filterType;
    QString filterAccount;
    Event::EventDirection filterDirection;
};

QString ConversationModelPrivate::FilterDefault::filterString() const
{
    QString filters;
    if (!filterAccount.isEmpty())
        filters += "AND Events.localUid = :filterAccount ";
    if (filterType != Event::UnknownType)
        filters += "AND Events.type = :filterType ";
    if (filterDirection != Event::UnknownDirection)
        filters += "AND Events.direction = :filterDirection ";
    return filters;
}

void ConversationModelPrivate::FilterDefault::bindValues(QSqlQuery &query) const
{
    if (!filterAccount.isEmpty())
        query.bindValue(":filterAccount", filterAccount);
    if (filterType != Event::UnknownType)
        query.bindValue(":filterType", filterType);
    if (filterDirection != Event::UnknownDirection)
        query.bindValue(":filterDirection", filterDirection);
}

bool ConversationModelPrivate::FilterDefault::acceptsEvent(const Event &event) const
{
    if (filterType != Event::UnknownType && event.type() != filterType)
        return false;

    if (!filterAccount.isEmpty() && event.localUid() != filterAccount)
        return false;

    if (filterDirection != Event::UnknownDirection && event.direction() != filterDirection)
        return false;

    return true;
}

class ConversationModelPrivate::FilterForMmsReadReport: public ConversationModelPrivate::Filter {
public:
    QString filterString() const;
    void bindValues(QSqlQuery &query) const;
    bool acceptsEvent(const Event &event) const;
};

QString ConversationModelPrivate::FilterForMmsReadReport::filterString() const
{
    return QString("AND Events.type = :filterType "
                   "AND Events.direction = :filterDirection "
                   "AND Events.isRead = :isRead "
                   "AND Events.reportRead = :reportRead "
                   "AND Events.mmsId != '' "
                   "AND Events.id IN ( "
                   "SELECT DISTINCT eventId from EventProperties WHERE EventProperties.key = :propertyKey ) ");
}

void ConversationModelPrivate::FilterForMmsReadReport::bindValues(QSqlQuery &query) const
{
    query.bindValue(":filterType", Event::MMSEvent);
    query.bindValue(":filterDirection", Event::Inbound);
    query.bindValue(":isRead", true);
    query.bindValue(":reportRead", true);
    query.bindValue(":propertyKey", QString(MMS_PROPERTY_IMSI));
}

bool ConversationModelPrivate::FilterForMmsReadReport::acceptsEvent(const Event &event) const
{
    return (event.type() == Event::MMSEvent &&
            event.direction() == Event::Inbound &&
            event.isRead() &&
            event.reportRead() &&
            !event.mmsId().isEmpty() &&
            !event.extraProperty(MMS_PROPERTY_IMSI).toString().isEmpty());
}

ConversationModelPrivate::ConversationModelPrivate(EventModel *model):
    EventModelPrivate(model),
    filter(NULL)
{
    QDBusConnection::sessionBus().connect(
        QString(), QString(), COMM_HISTORY_INTERFACE, GROUPS_DELETED_SIGNAL,
        this, SLOT(groupsDeletedSlot(const QList<int> &)));
    // remove call properties
    propertyMask -= unusedProperties;
}

ConversationModelPrivate::~ConversationModelPrivate()
{
    delete filter;
}

void ConversationModelPrivate::groupsDeletedSlot(const QList<int> &groupIds)
{
    Q_Q(ConversationModel);
    bool changed = false;

    foreach (int group, groupIds) {
        if (filterGroupIds.remove(group))
            changed = true;
    }

    // XXX This could be more efficient by removing events from this group without
    // refreshing others
    if (changed)
        q->getEvents(filterGroupIds.toList());
}

bool ConversationModelPrivate::acceptsEvent(const Event &event) const
{
    DEBUG() << __PRETTY_FUNCTION__ << event.id();
    if ((event.type() != Event::IMEvent
         && event.type() != Event::SMSEvent
         && event.type() != Event::MMSEvent
         && event.type() != Event::StatusMessageEvent))
        return false;

    if (event.isDraft())
        return false;

    if (filter && !filter->acceptsEvent(event))
        return false;

    if (!filterGroupIds.contains(event.groupId())) return false;

    DEBUG() << __PRETTY_FUNCTION__ << ": true";
    return true;
}

QSqlQuery ConversationModelPrivate::buildQuery() const
{
    QList<int> groups = filterGroupIds.toList();
    QString q;
    int unionCount = 0;

    qint64 firstTimestamp = 0;
    int firstId = -1;
    if (eventRootItem->childCount() > 0) {
        Event firstEvent = eventRootItem->eventAt(eventRootItem->childCount() - 1);
        firstTimestamp = firstEvent.endTime().toTime_t();
        firstId = firstEvent.id();
    }

    QString filters;
    if (filter)
        filters += filter->filterString();
    if (firstId >= 0) {
        filters += "AND (Events.endTime < :firstTimestamp OR (Events.endTime = :firstTimestamp "
                    "AND Events.id < :firstId)) ";
    }

    /* Rather than the intuitive solution of groupId IN (1,2),
     * this query is built as:
     *
     * SELECT .. FROM Events WHERE groupId=1
     * UNION ALL
     * SELECT .. FROM Events WHERE groupId=2
     *
     * Because SQLite is unable to use indexes for ORDER BY after a IN
     * or OR expression in the query, yet somehow is able to use that indexes
     * on the UNION ALL of these queries. This is true at least up to SQLite
     * 3.8.1. */
    do {
        if (unionCount)
            q += "UNION ALL ";
        q += DatabaseIOPrivate::eventQueryBase();
        q += "WHERE Events.isDraft = 0 ";

        if (unionCount < groups.size())
            q += "AND Events.groupId = " + QString::number(groups[unionCount]) + " ";

        q += filters;

        unionCount++;
    } while (unionCount < groups.size());

    q += "ORDER BY Events.endTime DESC, Events.id DESC ";

    if (queryLimit > 0)
        q += "LIMIT " + QString::number(queryLimit);
    else if (queryMode == EventModel::StreamedAsyncQuery && chunkSize > 0)
        q += "LIMIT " + QString::number((firstId < 0 && firstChunkSize > 0) ? firstChunkSize : chunkSize);

    QSqlQuery query = CommHistoryDatabase::prepare(q.toLatin1(), DatabaseIOPrivate::instance()->connection());
    if (filter)
        filter->bindValues(query);
    if (firstId >= 0) {
        query.bindValue(":firstTimestamp", firstTimestamp);
        query.bindValue(":firstId", firstId);
    }

    return query;
}

void ConversationModelPrivate::eventsReceivedSlot(int start, int end, QList<CommHistory::Event> events)
{
    // There is no more data when a query returns no rows
    if (queryMode == EventModel::StreamedAsyncQuery && events.size() == 0)
        isReady = true;

    EventModelPrivate::eventsReceivedSlot(start, end, events);
}

void ConversationModelPrivate::modelUpdatedSlot(bool successful)
{
    if (queryMode == EventModel::StreamedAsyncQuery) {
        if (isReady) {
            emit modelReady(successful);
        }
    } else{
        EventModelPrivate::modelUpdatedSlot(successful);
    }
}

bool ConversationModelPrivate::isModelReady() const
{
    return isReady;
}

ConversationModel::ConversationModel(QObject *parent)
        : EventModel(*new ConversationModelPrivate(this), parent)
{
}

ConversationModel::~ConversationModel()
{
}

bool ConversationModel::setFilter(Event::EventType type,
                                  const QString &account,
                                  Event::EventDirection direction)
{
    Q_D(ConversationModel);

    delete d->filter;
    d->filter = new ConversationModelPrivate::FilterDefault(type, account, direction);

    if (!d->filterGroupIds.isEmpty()) {
        return fetchEvents();
    }

    return true;
}

bool ConversationModel::setFilterForMmsReadReport()
{
    Q_D(ConversationModel);

    delete d->filter;
    d->filter = new ConversationModelPrivate::FilterForMmsReadReport();

    if (!d->filterGroupIds.isEmpty()) {
        return fetchEvents();
    }

    return true;
}

bool ConversationModel::getEvents(int groupId)
{
    return getEvents(QList<int>() << groupId);
}

bool ConversationModel::getEvents(QList<int> groupIds)
{
    Q_D(ConversationModel);

    QSet<int> ids = QSet<int>::fromList(groupIds);
    if (d->filterGroupIds != ids) {
        d->filterGroupIds = ids;
        return fetchEvents();
    }

    return true;
}

bool ConversationModel::fetchEvents()
{
    Q_D(ConversationModel);

    beginResetModel();
    d->clearEvents();
    endResetModel();

    if (d->filterGroupIds.isEmpty())
        return true;

    QSqlQuery query = d->buildQuery();
    return d->executeQuery(query);
}

bool ConversationModel::canFetchMore(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    Q_D(const ConversationModel);

    return !d->isModelReady();
}

void ConversationModel::fetchMore(const QModelIndex &parent)
{
    Q_UNUSED(parent);
    Q_D(ConversationModel);

    // isModelReady() is true when there are no more events to request
    if (d->isModelReady() || d->eventRootItem->childCount() < 1)
        return;

    QSqlQuery query = d->buildQuery();
    d->executeQuery(query);
}

}
