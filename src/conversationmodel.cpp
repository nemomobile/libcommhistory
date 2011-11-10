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

#include <QtDBus/QtDBus>
#include <QDebug>

#include "eventmodel_p.h"
#include "conversationmodel.h"
#include "conversationmodel_p.h"
#include "constants.h"
#include "eventsquery.h"
#include "queryrunner.h"
#include "contactlistener.h"

namespace {
static CommHistory::Event::PropertySet unusedProperties = CommHistory::Event::PropertySet()
                                             << CommHistory::Event::IsDraft
                                             << CommHistory::Event::IsMissedCall
                                             << CommHistory::Event::IsEmergencyCall
                                             << CommHistory::Event::BytesReceived
                                             << CommHistory::Event::EventCount;
}
namespace CommHistory {

ConversationModelPrivate::ConversationModelPrivate(EventModel *model)
            : EventModelPrivate(model)
            , filterGroupId(-1)
            , filterType(Event::UnknownType)
            , filterAccount(QString())
            , filterDirection(Event::UnknownDirection)
            , firstFetch(true)
            , eventsFilled(0)
            , lastEventTrackerId(0)
            , activeQueries(0)

{
    contactChangesEnabled = true;
    QDBusConnection::sessionBus().connect(
        QString(), QString(), "com.nokia.commhistory", "groupsUpdatedFull",
        this, SLOT(groupsUpdatedFullSlot(const QList<CommHistory::Group> &)));
    QDBusConnection::sessionBus().connect(
        QString(), QString(), "com.nokia.commhistory", GROUPS_DELETED_SIGNAL,
        this, SLOT(groupsDeletedSlot(const QList<int> &)));
    // remove call properties
    propertyMask -= unusedProperties;
}

void ConversationModelPrivate::groupsUpdatedFullSlot(const QList<CommHistory::Group> &groups)
{
    qDebug() << Q_FUNC_INFO;
    if (filterDirection == Event::Outbound
        || filterGroupId == -1
        || !propertyMask.contains(Event::Contacts))
        return;

    foreach (Group g, groups) {
        if (g.id() == filterGroupId && !g.remoteUids().isEmpty()) {
            // update in memory events for contact info
            updateEvents(g.contacts(),
                         g.remoteUids().first());
            break;
        }
    }
}

void ConversationModelPrivate::groupsDeletedSlot(const QList<int> &groupIds) {
    Q_Q(ConversationModel);

    if (filterGroupId != -1
        && groupIds.contains(filterGroupId)) {
        q->beginResetModel();
        clearEvents();
        q->endResetModel();
    }
}

// update contacts for all inbound events,
// should be called only for p2p chats
void ConversationModelPrivate::updateEvents(const QList<Event::Contact> &contacts,
                                            const QString &remoteUid)
{
    Q_Q(ConversationModel);

    for (int row = 0; row < eventRootItem->childCount(); row++) {
        Event &event = eventRootItem->eventAt(row);

        if (event.contacts() != contacts
            && event.direction() == Event::Inbound
            && event.remoteUid() == remoteUid) {
            //update and continue
            event.setContacts(contacts);

            emit q->dataChanged(q->createIndex(row,
                                               EventModel::Contacts,
                                               eventRootItem->child(row)),
                                q->createIndex(row,
                                               EventModel::Contacts,
                                               eventRootItem->child(row)));
        }
    }
}

bool ConversationModelPrivate::acceptsEvent(const Event &event) const
{
    qDebug() << __PRETTY_FUNCTION__ << event.id();
    if ((event.type() != Event::IMEvent
         && event.type() != Event::SMSEvent
         && event.type() != Event::MMSEvent
         && event.type() != Event::StatusMessageEvent) ||
        filterGroupId == -1) return false;

    if (filterType != Event::UnknownType &&
        event.type() != filterType) return false;

    if (!filterAccount.isEmpty() &&
        event.localUid() != filterAccount) return false;

    if (filterDirection != Event::UnknownDirection &&
        event.direction() != filterDirection) return false;

    if (filterGroupId != event.groupId()) return false;

    qDebug() << __PRETTY_FUNCTION__ << ": true";
    return true;
}

bool ConversationModelPrivate::fillModel(int start, int end, QList<CommHistory::Event> events)
{
    Q_UNUSED(start);
    Q_UNUSED(end);

    qDebug() << __FUNCTION__ << ": read" << events.count() << "messages";

    Q_Q(ConversationModel);

    q->beginInsertRows(QModelIndex(), q->rowCount(),
                       q->rowCount() + events.count() - 1);
    foreach (Event event, events) {
        eventRootItem->appendChild(new EventTreeItem(event, eventRootItem));
    }
    q->endInsertRows();

    eventsFilled += events.size();

    return true;
}

EventsQuery ConversationModelPrivate::buildQuery() const
{
    EventsQuery query(propertyMask);

    if (!filterAccount.isEmpty()) {
        query.addPattern(QString(QLatin1String("{%2 nmo:to [nco:hasContactMedium <telepathy:%1>]} "
                                               "UNION "
                                               "{%2 nmo:from [nco:hasContactMedium <telepathy:%1>]}"))
                         .arg(filterAccount))
                .variable(Event::Id);
    }

    if (filterType == Event::IMEvent) {
        query.addPattern(QLatin1String("%1 rdf:type nmo:IMMessage ."))
                        .variable(Event::Id);
    } else if (filterType == Event::SMSEvent) {
        query.addPattern(QLatin1String("%1 rdf:type nmo:SMSMessage ."))
                        .variable(Event::Id);
    }

    if (filterDirection == Event::Outbound) {
        query.addPattern(QLatin1String("%1 nmo:isSent \"true\" ."))
                        .variable(Event::Id);
    } else if (filterDirection == Event::Inbound) {
        query.addPattern(QLatin1String("%1 nmo:isSent \"false\" ."))
                         .variable(Event::Id);
    }

    query.addPattern(QLatin1String("%1 nmo:isDraft \"false\"; nmo:isDeleted \"false\" .")).variable(Event::Id);
    query.addPattern(QString(QLatin1String("%2 nmo:communicationChannel <%1> ."))
                     .arg(Group::idToUrl(filterGroupId).toString()))
            .variable(Event::Id);

    query.addModifier("ORDER BY DESC(%1) DESC(tracker:id(%2))")
                     .variable(Event::EndTime)
                     .variable(Event::Id);

    return query;
}

void ConversationModelPrivate::modelUpdatedSlot(bool successful)
{
    if (queryMode == EventModel::StreamedAsyncQuery) {
        activeQueries--;
        isReady = isModelReady();

        if (isReady) {
            if (successful) {
                if (messagePartsReady)
                    emit modelReady(true);
            } else {
                emit modelReady(false);
            }
        }
    } else{
        EventModelPrivate::modelUpdatedSlot(successful);
    }

    if (contactChangesEnabled && contactListener) {
        connect(contactListener.data(),
                SIGNAL(contactSettingsChanged(const QHash<QString, QVariant> &)),
                this,
                SLOT(contactSettingsChangedSlot(const QHash<QString, QVariant> &)),
                Qt::UniqueConnection);
    }
}

void ConversationModelPrivate::extraReceivedSlot(QList<CommHistory::Event> events,
                                                 QVariantList extra)
{
    Q_UNUSED(events);

    if (!extra.isEmpty()) {
        QVariant trackerId = extra.last();
        if (trackerId.isValid())
            lastEventTrackerId = trackerId.toInt();
    }
}

bool ConversationModelPrivate::isModelReady() const
{
    return activeQueries == 0
           && eventsFilled < (firstFetch ? firstChunkSize : chunkSize);
}

void ConversationModelPrivate::contactSettingsChangedSlot(const QHash<QString, QVariant> &changedSettings)
{
    Q_UNUSED(changedSettings);
    Q_Q(ConversationModel);

    if (filterGroupId != -1)
        q->getEvents(filterGroupId);
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

    d->filterType = type;
    d->filterAccount = account;
    d->filterDirection = direction;

    if (d->filterGroupId != -1) {
        return getEvents(d->filterGroupId);
    }

    return true;
}

bool ConversationModel::getEvents(int groupId)
{
    Q_D(ConversationModel);

    d->filterGroupId = groupId;

    beginResetModel();
    d->clearEvents();
    endResetModel();

    EventsQuery query = d->buildQuery();

    if (d->queryMode == EventModel::StreamedAsyncQuery) {
        d->isReady = false;
        query.addModifier(QLatin1String("LIMIT ") + QString::number(d->firstChunkSize));
        query.addProjection(QLatin1String("tracker:id(%1)")).variable(Event::Id);

        d->queryRunner->enableQueue();

        QString sparqlQuery = query.query();
        d->queryRunner->runEventsQuery(sparqlQuery, query.eventProperties());
        d->eventsFilled = 0;
        d->firstFetch = true;
        d->activeQueries = 1;

        connect(d->queryRunner, SIGNAL(eventsReceivedExtra(QList<CommHistory::Event>,QVariantList)),
                d, SLOT(extraReceivedSlot(QList<CommHistory::Event>,QVariantList)),
                Qt::UniqueConnection);

        d->queryRunner->startQueue();

        return true;
    }

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

    EventsQuery query = d->buildQuery();

    Event &event = d->eventRootItem->eventAt(d->eventRootItem->childCount() - 1);

    query.addProjection(QLatin1String("tracker:id(%1)")).variable(Event::Id);
    query.addPattern(QString(QLatin1String("FILTER (%3 < \"%1\"^^xsd:dateTime || (%3 = \"%1\"^^xsd:dateTime && tracker:id(%4) < %2))"))
                     .arg(event.endTime().toUTC().toString(Qt::ISODate)).arg(d->lastEventTrackerId))
        .variable(Event::EndTime)
        .variable(Event::Id);
    query.addModifier(QLatin1String("LIMIT ") + QString::number(d->chunkSize));

    QString sparqlQuery = query.query();
    d->queryRunner->runEventsQuery(sparqlQuery, query.eventProperties());
    d->eventsFilled = 0;
    d->firstFetch = false;
    d->activeQueries++;

    d->queryRunner->startQueue();
}

}
