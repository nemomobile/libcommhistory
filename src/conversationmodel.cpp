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

#include <QtDBus/QtDBus>
#include <QDebug>

#include "eventmodel_p.h"
#include "conversationmodel.h"
#include "conversationmodel_p.h"
#include "constants.h"
#include "eventsquery.h"

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
{
    contactChangesEnabled = true;
    QDBusConnection::sessionBus().connect(
        QString(), QString(), "com.nokia.commhistory", "groupsUpdatedFull",
        this, SLOT(groupsUpdatedFullSlot(const QList<CommHistory::Group> &)));
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
        if (g.id() == filterGroupId) {
            // update in memory events for contact info
            updateEvents(g.contacts(),
                         g.remoteUids().first());
            break;
        }
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

    return true;
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

    reset();
    d->clearEvents();

    EventsQuery query(d->propertyMask);

    if (!d->filterAccount.isEmpty()) {
        query.addPattern(QString(QLatin1String("{%2 nmo:to [nco:hasContactMedium <telepathy:%1>]} "
                                               "UNION "
                                               "{%2 nmo:from [nco:hasContactMedium <telepathy:%1>]}"))
                         .arg(d->filterAccount))
                .variable(Event::Id);
    }

    if (d->filterType == Event::IMEvent) {
        query.addPattern(QLatin1String("%1 rdf:type nmo:IMMessage ."))
                        .variable(Event::Id);
    } else if (d->filterType == Event::SMSEvent) {
        query.addPattern(QLatin1String("%1 rdf:type nmo:SMSMessage ."))
                        .variable(Event::Id);
    }

    if (d->filterDirection == Event::Outbound) {
        query.addPattern(QLatin1String("%1 nmo:isSent \"true\" ."))
                        .variable(Event::Id);
    } else if (d->filterDirection == Event::Inbound) {
        query.addPattern(QLatin1String("%1 nmo:isSent \"false\" ."))
                         .variable(Event::Id);
    }

    query.addPattern(QLatin1String("%1 nmo:isDraft \"false\"; nmo:isDeleted \"false\" .")).variable(Event::Id);
    query.addPattern(QString(QLatin1String("%2 nmo:communicationChannel <%1> ."))
                     .arg(Group::idToUrl(groupId).toString()))
            .variable(Event::Id);

    query.addModifier("ORDER BY DESC(%1) DESC(tracker:id(%2))")
                     .variable(Event::EndTime)
                     .variable(Event::Id);

    return d->executeQuery(query);
}

}
