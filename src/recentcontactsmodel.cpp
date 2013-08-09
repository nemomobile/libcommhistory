/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd. <matthew.vogt@jollamobile.com>
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

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

#include "databaseio_p.h"
#include "commhistorydatabase.h"
#include "eventmodel_p.h"
#include "contactlistener.h"

#include "recentcontactsmodel.h"

namespace CommHistory {

using namespace CommHistory;

static const QString imAccountType(QString::fromLatin1("accountUri"));
static const QString phoneNumberType(QString::fromLatin1("phoneNumber"));
static const QString emailAddressType(QString::fromLatin1("emailAddress"));

static int eventContact(const Event &event)
{
    return event.contacts().first().first;
}

class RecentContactsModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(RecentContactsModel);

    RecentContactsModelPrivate(EventModel *model)
        : EventModelPrivate(model),
          selectionProperty(ContactListener::UnknownType)
    {
        contactChangesEnabled = true;
    }

    bool fillModel(int start, int end, QList<Event> events);

    void eventsAddedSlot(const QList<Event> &events);
    void eventsUpdatedSlot(const QList<Event> &events);

    void slotContactUpdated(quint32 localId,
                            const QString &contactName,
                            const QList<ContactAddress> &contactAddresses);
    void slotContactRemoved(quint32 localId);
    void slotContactUnknown(const QPair<QString, QString> &address);

private:
    void updatePendingEvents(const QList<Event> &events);
    void updatePendingEvent(Event *event);

    void prependEvent(const Event &event);
    void prependEvents(const QList<Event> &events);

    void pendingEventsResolved();

    bool skipIrrelevantContact(const Event &event);

    ContactListener::ContactAddressType selectionProperty;
    mutable QList<Event> pendingEvents;
};

bool RecentContactsModelPrivate::fillModel(int start, int end, QList<Event> events)
{
    Q_UNUSED(end)

    bool unresolved = false;
    if (!events.isEmpty()) {
        for (QList<Event>::iterator it = events.begin(); it != events.end(); ) {
            const Event &event(*it);
            if (event.contacts().isEmpty()) {
                unresolved = true;
            } else if (skipIrrelevantContact(event)) {
                it = events.erase(it);
                continue;
            }
            ++it;
        }

        if (unresolved) {
            // We need to resolve all contacts before filling the model
            pendingEvents.append(events);
            return false;
        }

        // Nothing to resolve
        int count = queryLimit ? qMin(events.count(), queryLimit) : events.count();
        return EventModelPrivate::fillModel(start, start + count - 1, events.mid(0, count));
    }

    return false;
}

void RecentContactsModelPrivate::eventsAddedSlot(const QList<Event> &events)
{
    EventModelPrivate::eventsAddedSlot(events);

    updatePendingEvents(events);
}

void RecentContactsModelPrivate::eventsUpdatedSlot(const QList<Event> &events)
{
    EventModelPrivate::eventsUpdatedSlot(events);

    updatePendingEvents(events);
}

void RecentContactsModelPrivate::slotContactUpdated(quint32 localId,
                                                    const QString &contactName,
                                                    const QList<ContactAddress> &contactAddresses)
{
    EventModelPrivate::slotContactUpdated(localId, contactName, contactAddresses);

    if (!pendingEvents.isEmpty()) {
        // See if we have resolved our pending events
        bool unresolved = false;
        for (QList<Event>::iterator it = pendingEvents.begin(); it != pendingEvents.end(); ) {
            Event &event(*it);

            if (event.contacts().isEmpty()) {
                if (ContactListener::addressMatchesList(event.localUid(),
                                                        event.remoteUid(),
                                                        contactAddresses)) {
                    setContactFromCache(event);

                    if (skipIrrelevantContact(event)) {
                        it = pendingEvents.erase(it);
                        continue;
                    }
                } else {
                    unresolved = true;
                }
            }

            ++it;
        }

        if (!unresolved) {
            pendingEventsResolved();
        }
    }
}

void RecentContactsModelPrivate::slotContactRemoved(quint32 localId)
{
    EventModelPrivate::slotContactRemoved(localId);

    // Remove any event for this contact (there can only be one)
    const int rowCount = eventRootItem->childCount();
    for (int row = 0; row < rowCount; ++row) {
        const Event &existing(eventRootItem->eventAt(row));
        if (existing.contacts().isEmpty()) {
            deleteFromModel(existing.id());
            break;
        }
    }
}

void RecentContactsModelPrivate::slotContactUnknown(const QPair<QString, QString> &address)
{
    if (!pendingEvents.isEmpty()) {
        // Remove any events with this address
        bool unresolved = false;
        for (QList<Event>::iterator it = pendingEvents.begin(); it != pendingEvents.end(); ) {
            Event &event(*it);

            QPair<QString, QString> eventAddress = qMakePair(event.localUid(), event.remoteUid());
            if (eventAddress == address) {
                qDebug() << "Could not resolve contact address:" << address;
                it = pendingEvents.erase(it);
            } else {
                if (event.contacts().isEmpty()) {
                    unresolved = true;
                }
                ++it;
            }
        }

        if (!unresolved && !pendingEvents.isEmpty()) {
            pendingEventsResolved();
        }
    }
}

void RecentContactsModelPrivate::updatePendingEvents(const QList<Event> &events)
{
    QList<Event> newEvents(events);
    QList<Event>::iterator it = newEvents.begin(), end = newEvents.end();
    for ( ; it != end; ++it) {
        updatePendingEvent(&*it);
    }
}

void RecentContactsModelPrivate::updatePendingEvent(Event *event)
{
    if (event->contacts().isEmpty()) {
        // Fetch or request contact information for this event
        setContactFromCache(*event);
    }

    bool addEvent = true;
    if (!event->contacts().isEmpty() && skipIrrelevantContact(*event)) {
        // Ignore this contact/event
        addEvent = false;
    }

    if (!pendingEvents.isEmpty()) {
        bool unresolved = false;
        for (QList<Event>::iterator it = pendingEvents.begin(); it != pendingEvents.end(); ) {
            Event &pending(*it);
            if (pending.id() == event->id()) {
                // This event is already pending - this is an update
                pending = *event;
                addEvent = false;
            }

            if (pending.contacts().isEmpty()) {
                unresolved = true;
            } else if (skipIrrelevantContact(pending)) {
                it = pendingEvents.erase(it);
                continue;
            }
            ++it;
        }

        if (addEvent) {
            // With previous events not yet added, we can't insert this one out of order
            pendingEvents.prepend(*event);
            if (!unresolved && event->contacts().isEmpty()) {
                unresolved = true;
            }
        }

        if (!unresolved) {
            // All events are now resolved
            const_cast<RecentContactsModelPrivate *>(this)->pendingEventsResolved();
        }
    } else if (addEvent) {
        pendingEvents.prepend(*event);
        if (!event->contacts().isEmpty()) {
            const_cast<RecentContactsModelPrivate *>(this)->pendingEventsResolved();
        }
    }
}

void RecentContactsModelPrivate::pendingEventsResolved()
{
    if (!pendingEvents.isEmpty()) {
        // We have resolved all contacts
        prependEvents(pendingEvents);
        pendingEvents.clear();
    }
}

void RecentContactsModelPrivate::prependEvent(const Event &event)
{
    prependEvents(QList<Event>() << event);
}

void RecentContactsModelPrivate::prependEvents(const QList<Event> &events)
{
    Q_Q(RecentContactsModel);

    // Ensure the new events represent different contacts
    QList<Event> newEvents;
    QSet<int> newContactIds;
    foreach (const Event &event, events) {
        const int eventContactId = eventContact(event);
        if (!newContactIds.contains(eventContactId)) {
            newContactIds.insert(eventContactId);
            newEvents.append(event);

            // Don't add any more events than we can present
            if (newEvents.count() == queryLimit) {
                break;
            }
        }
    }

    QSet<int> removeSet;

    // Does the new event replace an existing event?
    const int rowCount = eventRootItem->childCount();
    for (int row = 0; row < rowCount; ++row) {
        const Event &existing(eventRootItem->eventAt(row));
        if (newContactIds.contains(eventContact(existing))) {
            removeSet.insert(row);
        }
    }

    // Do we need to remove the final event(s) to maintain the limit?
    if (queryLimit) {
        int trimCount = rowCount + newEvents.count() - removeSet.count() - queryLimit;
        int removeIndex = rowCount - 1;
        while (trimCount > 0) {
            while (removeSet.contains(removeIndex)) {
                --removeIndex;
            }
            if (removeIndex < 0) {
                break;
            } else {
                removeSet.insert(removeIndex);
                --removeIndex;
                --trimCount;
            }
        }
    }

    // Remove the rows that have been made obsolete
    QList<int> removeIndices = removeSet.toList();
    qSort(removeIndices);

    int count;
    while ((count = removeIndices.count()) != 0) {
        int end = removeIndices.last();
        int consecutiveCount = 1;
        for ( ; (count - consecutiveCount) > 0; ++consecutiveCount) {
            if (removeIndices.at(count - 1 - consecutiveCount) != (end - consecutiveCount)) {
                break;
            }
        }

        removeIndices = removeIndices.mid(0, count - consecutiveCount);

        int start = (end - consecutiveCount + 1);
        q->beginRemoveRows(QModelIndex(), start, end);
        while (end >= start) {
            eventRootItem->removeAt(end);
            --end;
        }
        q->endRemoveRows();
    }

    // Insert the new events at the start
    int start = 0;
    q->beginInsertRows(QModelIndex(), start, newEvents.count() - 1);
    QList<Event>::const_iterator it = newEvents.constBegin(), end = newEvents.constEnd();
    for ( ; it != end; ++it) {
        eventRootItem->insertChildAt(start++, new EventTreeItem(*it, eventRootItem));
    }
    q->endInsertRows();
}

bool RecentContactsModelPrivate::skipIrrelevantContact(const Event &event)
{
    if (selectionProperty != ContactListener::UnknownType) {
        int contactId = eventContact(event);
        if (!contactHasAddressType(selectionProperty, contactId)) {
            return true;
        }
    }

    return false;
}

RecentContactsModel::RecentContactsModel(QObject *parent)
    : EventModel(*new RecentContactsModelPrivate(this), parent)
{
}

RecentContactsModel::~RecentContactsModel()
{
}

QString RecentContactsModel::selectionProperty() const
{
    Q_D(const RecentContactsModel);

    return d->selectionProperty == ContactListener::IMAccountType
                                ? imAccountType
                                : (d->selectionProperty == ContactListener::PhoneNumberType
                                                        ? phoneNumberType
                                                        : (d->selectionProperty == ContactListener::EmailAddressType
                                                                                ? emailAddressType
                                                                                : QString()));
}

void RecentContactsModel::setSelectionProperty(const QString &name)
{
    Q_D(RecentContactsModel);

    if (name == imAccountType) {
        d->selectionProperty = ContactListener::IMAccountType;
    } else if (name == phoneNumberType) {
        d->selectionProperty = ContactListener::PhoneNumberType;
    } else if (name == emailAddressType) {
        d->selectionProperty = ContactListener::EmailAddressType;
    } else {
        d->selectionProperty = ContactListener::UnknownType;
        if (!name.isEmpty()) {
            qWarning() << "Unknown selection property type:" << name;
        }
    }
}

bool RecentContactsModel::getEvents()
{
    Q_D(RecentContactsModel);

    beginResetModel();
    d->clearEvents();
    endResetModel();

    QString limitClause;
    if (d->queryLimit) {
        // Default to twice the configured limit, because some of the addresses may
        // resolve to the same final contact
        limitClause = QString::fromLatin1(" LIMIT %1").arg(2 * d->queryLimit);
    }

    QString q = DatabaseIOPrivate::eventQueryBase() + QString::fromLatin1(
" WHERE Events.id IN ("
  " SELECT lastId FROM ("
    " SELECT max(id) AS lastId, max(startTime) FROM Events"
    " JOIN ("
      " SELECT remoteUid, localUid, max(startTime) AS lastEventTime FROM Events"
      " GROUP BY remoteUid, localUid"
      " ORDER BY lastEventTime DESC %1"
    " ) AS LastEvent ON Events.startTime = LastEvent.lastEventTime"
                   " AND Events.remoteUid = LastEvent.remoteUid"
                   " AND Events.localUid = LastEvent.localUid"
    " GROUP BY Events.remoteUid, Events.localUid"
    " ORDER BY max(startTime) DESC"
  " )"
" )"
" ORDER BY Events.startTime DESC").arg(limitClause);

    QSqlQuery query = DatabaseIOPrivate::instance()->createQuery();
    if (!query.prepare(q)) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return d->executeQuery(query);
}

} // namespace CommHistory
