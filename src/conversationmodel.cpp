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
            , chatType(Group::ChatTypeP2P)
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
        || chatType != Group::ChatTypeP2P
        || !propertyMask.contains(Event::ContactId))
        return;

    foreach (Group g, groups) {
        if (g.id() == filterGroupId) {
            // update in memory events for contact info
            updateEventsRecursive(g.contactId(),
                                  g.contactName(),
                                  g.remoteUids().first(),
                                  eventRootItem);
            break;
        }
    }
}

// update contactId and contactName for all inbound events,
// should be called only for p2p chats
// return true to abort when an event already have the same contact id/name
bool ConversationModelPrivate::updateEventsRecursive(int contactId,
                                                     const QString &contactName,
                                                     const QString &remoteUid,
                                                     EventTreeItem *parent)
{
    Q_Q(ConversationModel);

    for (int row = 0; row < parent->childCount(); row++) {
        Event &event = parent->eventAt(row);

        if (parent->child(row)->childCount()) {
            if (updateEventsRecursive(contactId,
                                      contactName,
                                      remoteUid,
                                      parent->child(row)))
                return true;
        } else {
            if (event.contactId() == contactId
                && event.contactName() == contactName) {
                // if we found event with same info, abort
                return true;
            } else if (event.direction() == Event::Inbound
                       && event.remoteUid() == remoteUid) {
                //update and continue
                event.setContactId(contactId);
                event.setContactName(contactName);

                emit q->dataChanged(q->createIndex(row,
                                                   EventModel::ContactId,
                                                   parent->child(row)),
                                    q->createIndex(row,
                                                   EventModel::ContactName,
                                                   parent->child(row)));
            }
        }
    }
    return false;
}

QModelIndex ConversationModelPrivate::findParent(const Event &event)
{
    qDebug() << __PRETTY_FUNCTION__;
    Q_Q(ConversationModel);
    // TODO: find the correct insertion spot by date
    Q_UNUSED(event);
    if (isInTreeMode) {
        // check if the first branch is for today's events
        QDateTime today = QDateTime::currentDateTime();
        today.setTime(QTime(0, 0));
        if (eventRootItem->childCount() &&
            eventRootItem->eventAt(0).startTime() == today) {
            return q->index(0, 0, QModelIndex());
        }

        // nope, insert branch
        q->beginInsertRows(QModelIndex(), 0, 0);
        Event divider;
        divider.setStartTime(today);
        divider.setEndTime(QDateTime::fromTime_t(UINT_MAX));
        divider.setFreeText(DIVIDER_TODAY);
        eventRootItem->prependChild(new EventTreeItem(divider, eventRootItem));
        q->endInsertRows();

        return q->index(0, 0, QModelIndex());
    } else {
        return QModelIndex();
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

    if (!isInTreeMode) {
        q->beginInsertRows(QModelIndex(), q->rowCount(),
                           q->rowCount() + events.count() - 1);
        foreach (Event event, events) {
            eventRootItem->appendChild(new EventTreeItem(event, eventRootItem));
        }
        q->endInsertRows();
    } else {
        QMap<EventTreeItem *, QList<Event> > branches;
        foreach (Event event, events) {
            EventTreeItem *parent = findDivider(event);
            branches[parent].append(event);
        }

        QMapIterator<EventTreeItem *, QList<Event> > i(branches);
        while (i.hasNext()) {
            i.next();
            EventTreeItem *parent = i.key();
            q->beginInsertRows(q->createIndex(0, 0, parent),
                               parent->childCount(),
                               parent->childCount() + i.value().count() - 1);
            foreach (Event e, i.value()) {
                parent->appendChild(new EventTreeItem(e, parent));
            }
            q->endInsertRows();
        }
    }

    return true;
}

// Find, and create if necessary, the corresponding date divider node.
EventTreeItem* ConversationModelPrivate::findDividerItem(const Event &divider)
{
    Q_Q(ConversationModel);
    EventTreeItem *item = 0;

    // if an exact match is found, set the item. Otherwise, stop at
    // the row where we can insert the new divider.
    int row;
    for (row = 0; row < eventRootItem->childCount(); row++) {
        if (eventRootItem->eventAt(row).startTime() == divider.startTime()) {
            return eventRootItem->child(row);
        }
        if (divider.startTime() > eventRootItem->eventAt(row).startTime()) break;
    }

    q->beginInsertRows(QModelIndex(), row, row);
    item = new EventTreeItem(divider, eventRootItem);
    eventRootItem->insertChildAt(row, item);
    q->endInsertRows();

    return item;
}

EventTreeItem* ConversationModelPrivate::findDivider(const Event &event)
{
    Event divider;

    QDateTime today = QDateTime::currentDateTime();
    today.setTime(QTime(0, 0));
    if (event.endTime() >= today) {
        divider.setStartTime(today);
        divider.setEndTime(QDateTime::fromTime_t(UINT_MAX));
        divider.setFreeText(DIVIDER_TODAY);
        return findDividerItem(divider);
    }

    QDateTime yesterday = today.addDays(-1);
    if (event.endTime() >= yesterday) {
        divider.setStartTime(yesterday);
        divider.setEndTime(today);
        divider.setFreeText(DIVIDER_YESTERDAY);
        return findDividerItem(divider);
    }

    for (int days = 2; days < 7; days++) {
        QDateTime daysAgo = today.addDays(-days);
        if (event.endTime() >= daysAgo) {
            divider.setStartTime(daysAgo);
            divider.setEndTime(daysAgo.addDays(1));
            divider.setFreeText(daysAgo.date().toString(Qt::DefaultLocaleLongDate));
            return findDividerItem(divider);
        }
    }

    for (int weeks = 1; weeks < 4; weeks++) {
        QDateTime weeksAgo = today.addDays(-7 * weeks);
        if (event.endTime() >= weeksAgo) {
            divider.setStartTime(weeksAgo);
            divider.setEndTime(weeksAgo.addDays(7));
            // TODO: l10n - set some field and let UI do the text?
            divider.setFreeText(QString(DIVIDER_N_WEEKS_AGO).arg(weeks));
            return findDividerItem(divider);
        }
    }

    divider.setStartTime(today.addDays(-7 * 4));
    divider.setEndTime(today.addDays(-7 * 3));
    for (int months = 1; months < 6; months++) {
        QDateTime monthsAgo = divider.startTime().addMonths(-months);
        if (event.endTime() >= monthsAgo) {
            divider.setStartTime(monthsAgo);
            QDateTime end = divider.endTime().addMonths(-months);
            divider.setEndTime(end);
            // TODO: l10n - set some field and let UI do the text?
            divider.setFreeText(QString(DIVIDER_N_MONTHS_AGO).arg(months));
            return findDividerItem(divider);
        }
    }

    divider.setStartTime(QDateTime::fromTime_t(0));
    divider.setEndTime(divider.endTime().addMonths(-5));
    divider.setFreeText(DIVIDER_OLDER);
    return findDividerItem(divider);
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
        return getEvents(d->filterGroupId, d->chatType);
    }

    return true;
}

bool ConversationModel::getEvents(int groupId, Group::ChatType chatType)
{
    Q_D(ConversationModel);

    d->filterGroupId = groupId;

    reset();
    d->clearEvents();

    EventsQuery query(d->propertyMask);

    if (chatType == Group::ChatTypeP2P) {
        query.addPattern(QLatin1String(
                "{"
                "SELECT tracker:id(?contact) AS %2 "
                "fn:string-join((nco:nameGiven(?contact), nco:nameFamily(?contact), nco:imNickname(?imAddress)),\",\") AS %3 "
                "WHERE { OPTIONAL {"
                "{?contact nco:hasAffiliation [nco:hasIMAddress ?imAddress] ."
                " %1 nmo:hasParticipant [nco:hasIMAddress ?imAddress] . "
                "} "
                "UNION "
                "{?contact nco:hasAffiliation [nco:hasPhoneNumber [ maemo:localPhoneNumber ?contactPhone]] ."
                " %1 nmo:hasParticipant [nco:hasPhoneNumber [maemo:localPhoneNumber ?contactPhone]] .} "
                "?contact rdf:type nco:PersonContact . "
                "}}}"))
                .variable(Event::GroupId)
                .variable(Event::ContactId)
                .variable(Event::ContactName);
    } else if (chatType == Group::ChatTypeUnnamed
               || chatType == Group::ChatTypeRoom) {
        query.addPattern(QLatin1String(
                "{"
                "SELECT tracker:id(?contact) AS %2 "
                "fn:string-join((nco:nameGiven(?contact), nco:nameFamily(?contact), nco:imNickname(?imAddress)),\",\") AS %3 "
                "WHERE { OPTIONAL {"
                "?contact nco:hasAffiliation [nco:hasIMAddress ?imAddress] ."
                "{ %1 nmo:to [nco:hasIMAddress ?imAddress]; nmo:isSent \"true\"} "
                "UNION "
                "{ %1 nmo:from [nco:hasIMAddress ?imAddress]; nmo:isSent \"false\"} "
                "?contact rdf:type nco:PersonContact . "
                "}}}"))
                .variable(Event::Id)
                .variable(Event::ContactId)
                .variable(Event::ContactName);
    } else {
        qWarning() << Q_FUNC_INFO << ": unsupported chat type" << chatType << "???";
        return false;
    }

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
