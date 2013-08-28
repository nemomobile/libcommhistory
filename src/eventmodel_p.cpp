/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd. <john.brooks@jollamobile.com>
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
#include <QSqlQuery>
#include <QSqlError>

#include "databaseio.h"
#include "databaseio_p.h"
#include "eventmodel.h"
#include "eventmodel_p.h"
#include "updatesemitter.h"
#include "event.h"
#include "eventtreeitem.h"
#include "constants.h"
#include "commonutils.h"
#include "debug.h"
#include "recentcontactsmodel.h"

using namespace CommHistory;

namespace {
    static const int defaultChunkSize = 50;
}

EventModelPrivate::EventModelPrivate(EventModel *model)
        : isInTreeMode(false)
        , queryMode(EventModel::AsyncQuery)
        , chunkSize(defaultChunkSize)
        , firstChunkSize(0)
        , queryLimit(0)
        , queryOffset(0)
        , isReady(true)
        , messagePartsReady(true)
        , threadCanFetchMore(false)
        , contactChangesEnabled(false)
        , propertyMask(Event::allProperties())
        , bgThread(0)
{
    q_ptr = model;
    qRegisterMetaType<QList<CommHistory::Event> >();
    // emit dbus signals
    emitter = UpdatesEmitter::instance();
    connect(this, SIGNAL(eventsAdded(const QList<CommHistory::Event>&)),
            emitter.data(), SIGNAL(eventsAdded(const QList<CommHistory::Event>&)));
    connect(this, SIGNAL(eventsUpdated(const QList<CommHistory::Event>&)),
            emitter.data(), SIGNAL(eventsUpdated(const QList<CommHistory::Event>&)));
    connect(this, SIGNAL(eventDeleted(int)),
            emitter.data(), SIGNAL(eventDeleted(int)));
    connect(this, SIGNAL(groupsUpdated(const QList<int>&)),
            emitter.data(), SIGNAL(groupsUpdated(const QList<int>&)));
    connect(this, SIGNAL(groupsUpdatedFull(const QList<CommHistory::Group>&)),
            emitter.data(), SIGNAL(groupsUpdatedFull(const QList<CommHistory::Group>&)));
    connect(this, SIGNAL(groupsDeleted(const QList<int>&)),
            emitter.data(), SIGNAL(groupsDeleted(const QList<int>&)));

    // listen to dbus signals
    QDBusConnection::sessionBus().connect(
        QString(), QString(), COMM_HISTORY_SERVICE_NAME, EVENTS_ADDED_SIGNAL,
        this, SLOT(eventsAddedSlot(const QList<CommHistory::Event> &)));
    QDBusConnection::sessionBus().connect(
        QString(), QString(), COMM_HISTORY_SERVICE_NAME, EVENTS_UPDATED_SIGNAL,
        this, SLOT(eventsUpdatedSlot(const QList<CommHistory::Event> &)));
    QDBusConnection::sessionBus().connect(
        QString(), QString(), COMM_HISTORY_SERVICE_NAME, EVENT_DELETED_SIGNAL,
        this, SLOT(eventDeletedSlot(int)));

    eventRootItem = new EventTreeItem(Event());
}

EventModelPrivate::~EventModelPrivate()
{
    DEBUG() << Q_FUNC_INFO;

    delete eventRootItem;
}

bool EventModelPrivate::acceptsEvent(const Event &event) const
{
    Q_UNUSED(event);
    return false;
}

QModelIndex EventModelPrivate::findEventRecursive(int id, EventTreeItem *parent) const
{
    Q_Q(const EventModel);

    for (int row = 0; row < parent->childCount(); row++) {
        if (parent->eventAt(row).id() == id) {
            return q->createIndex(row, 0, parent->child(row));
        } else if (parent->child(row)->childCount()) {
            return findEventRecursive(id, parent->child(row));
        }
    }
    return QModelIndex();
}

QModelIndex EventModelPrivate::findEvent(int id) const
{
    return findEventRecursive(id, eventRootItem);
}

QModelIndex EventModelPrivate::findParent(const Event &event)
{
    Q_UNUSED(event);
    return QModelIndex();
}

bool EventModelPrivate::executeQuery(QSqlQuery &query)
{
    DEBUG() << __PRETTY_FUNCTION__;

    startContactListening();

    isReady = false;

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
    }

    QList<Event> events;
    while (query.next()) {
        Event e;
        DatabaseIOPrivate::readEventResult(query, e);
        events.append(e);
    }

    eventsReceivedSlot(0, events.size(), events);
    modelUpdatedSlot(true);
    return true;
}

bool EventModelPrivate::fillModel(int start,
                                 int end,
                                 QList<CommHistory::Event> events)
{
    Q_UNUSED(start);
    Q_UNUSED(end);

    Q_Q(EventModel);
    DEBUG() << __PRETTY_FUNCTION__ << ": read" << events.count() << "events";

    q->beginInsertRows(QModelIndex(), q->rowCount(), q->rowCount() + events.count() - 1);
    foreach (Event event, events) {
        eventRootItem->appendChild(new EventTreeItem(event, eventRootItem));
    }
    q->endInsertRows();

    return false;
}

void EventModelPrivate::clearEvents()
{
    DEBUG() << __PRETTY_FUNCTION__;
    delete eventRootItem;
    eventRootItem = new EventTreeItem(Event());
}

void EventModelPrivate::addToModel(Event &event)
{
    Q_Q(EventModel);
    DEBUG() << Q_FUNC_INFO << event.toString();

    if (!event.contacts().isEmpty()) {
        contactCache.insert(qMakePair(event.localUid(), event.remoteUid()), event.contacts());
    } else {
        setContactFromCache(event);
    }

    QModelIndex index = findParent(event);
    if (index.isValid()) {
        q->beginInsertRows(index, index.row(), index.row());
    } else {
        q->beginInsertRows(QModelIndex(), 0, 0);
    }
    EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
    if (!item) item = eventRootItem;
    item->prependChild(new EventTreeItem(event, item));
    q->endInsertRows();
}

void EventModelPrivate::modifyInModel(Event &event)
{
    Q_Q(EventModel);
    DEBUG() << __PRETTY_FUNCTION__ << event.id();

    if (!event.validProperties().contains(Event::Contacts)) {
        setContactFromCache(event);
    }

    QModelIndex index = findEvent(event.id());
    if (index.isValid()) {
        EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
        Event oldEvent = item->event();
        QDateTime oldTime = oldEvent.endTime();
        oldEvent.copyValidProperties(event);
        item->setEvent(oldEvent);

        // move event if endTime has changed
        if (index.row() > 0 && oldTime < event.endTime()) {
            EventTreeItem *parent = item->parent();
            if (!parent)
                parent = eventRootItem;
            // TODO: beginMoveRows if/when view supports it
            emit q->layoutAboutToBeChanged();
            parent->moveChild(index.row(), 0);
            emit q->layoutChanged();
        } else {
            emitDataChanged(index.row(), index.internalPointer());
        }
    }
}

void EventModelPrivate::deleteFromModel(int id)
{
    Q_Q(EventModel);
    DEBUG() << __PRETTY_FUNCTION__ << id;
    QModelIndex index = findEvent(id);
    if (index.isValid()) {
        q->beginRemoveRows(index.parent(), index.row(), index.row());
        EventTreeItem *parent = static_cast<EventTreeItem *>(index.parent().internalPointer());
        if (!parent) parent = eventRootItem;
        parent->removeAt(index.row());
        q->endRemoveRows();
    }
}

void EventModelPrivate::eventsReceivedSlot(int start, int end, QList<Event> events)
{
    DEBUG() << __PRETTY_FUNCTION__ << ":" << start << end << events.count();

    QMutableListIterator<Event> i(events);
    while (i.hasNext()) {
        Event event = i.next();

        if (findEvent(event.id()).isValid()) {
            end--;
            i.remove();
            continue;
        }

        if (!event.contacts().isEmpty()) {
            contactCache.insert(qMakePair(event.localUid(), event.remoteUid()), event.contacts());
        }  else {
            setContactFromCache(event);
        }
    }

    if (!events.isEmpty()) {
        // now we have some content -> start tracking contacts if enabled
        startContactListening();

        fillModel(start, end, events);
    }
}

void EventModelPrivate::messagePartsReceivedSlot(int eventId,
                                                 QList<CommHistory::MessagePart> parts)
{
    DEBUG() << __PRETTY_FUNCTION__ << ":" << eventId << parts.count();

    QModelIndex index = findEvent(eventId);
    if (index.isValid()) {
        EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
        QList<CommHistory::MessagePart> newParts = item->event().messageParts() + parts;
        item->event().setMessageParts(newParts);
        item->event().resetModifiedProperty(Event::MessageParts);

        emitDataChanged(index.row(), index.internalPointer());
    }
}

void EventModelPrivate::modelUpdatedSlot(bool successful)
{
    DEBUG() << __PRETTY_FUNCTION__;

    isReady = true;
    if (successful) {
        if (messagePartsReady)
            emit modelReady(true);
    } else {
        emit modelReady(false);
    }
}

void EventModelPrivate::partsUpdatedSlot(bool successful)
{
    DEBUG() << __PRETTY_FUNCTION__;

    messagePartsReady = true;
    if (successful) {
        if (isReady)
            emit modelReady(true);
    } else {
        emit modelReady(false);
    }
}

void EventModelPrivate::eventsAddedSlot(const QList<Event> &events)
{
    DEBUG() << __PRETTY_FUNCTION__ << ":" << events.count() << "events";

    foreach (const Event &event, events) {
        QModelIndex index = findEvent(event.id());
        if (index.isValid()) return;

        Event e = event;
        if (acceptsEvent(e))
            addToModel(e);
    }
}

void EventModelPrivate::eventsUpdatedSlot(const QList<Event> &events)
{
    DEBUG() << __PRETTY_FUNCTION__ << ":" << events.count();

    foreach (const Event &event, events) {
        QModelIndex index = findEvent(event.id());
        Event e = event;

        if (!index.isValid()) {
            if (acceptsEvent(e))
                addToModel(e);

            continue;
        }

        modifyInModel(e);
    }
}

void EventModelPrivate::eventDeletedSlot(int id)
{
    DEBUG() << __PRETTY_FUNCTION__ << ":" << id;

    deleteFromModel(id);
}

void EventModelPrivate::canFetchMoreChangedSlot(bool canFetch)
{
    threadCanFetchMore = canFetch;
}

bool EventModelPrivate::canFetchMore() const
{
    //return threadCanFetchMore;
    return false;
}

void EventModelPrivate::changeContactsRecursive(ContactChangeType changeType,
                                                quint32 contactId,
                                                const QString &contactName,
                                                const QList< QPair<QString,QString> > &contactAddresses,
                                                EventTreeItem *parent)
{
    DEBUG() << Q_FUNC_INFO;

    for (int row = 0; row < parent->childCount(); row++) {

        Event *event = &(parent->eventAt(row));
        bool eventChanged = false;
        QList<Event::Contact> contacts = event->contacts();
        bool addressMatchesList = ContactListener::addressMatchesList(event->localUid(),
                                                                      event->remoteUid(),
                                                                      contactAddresses);

        // the contact was removed
        if (changeType == ContactRemoved ||
        // the contact was modified and address removed
            (changeType == ContactUpdated && !addressMatchesList)) {

            for (int i = 0; i < contacts.count(); i++) {
                // if contact is already resolved, remove it from list
                if ((quint32)contacts.at(i).first == contactId) {

                    contacts.removeAt(i);
                    eventChanged = true;
                    break;
                }
            }
        }

        // the contact was modified and the address was found
        else if (changeType == ContactUpdated && addressMatchesList) {

            // create new contact, i.e. <id, name> pair
            Event::Contact newContact((int)contactId, contactName);
            // create cache key
            QPair<QString, QString> cacheKey = qMakePair(event->localUid(), event->remoteUid());
            contactCache.insert(cacheKey, QList<Event::Contact>() << newContact);

            for (int i = 0; i < contacts.count(); i++) {
                // if contact is already resolved, change name to new one
                if ((quint32)contacts.at(i).first == contactId) {

                    contacts[i].second = contactName;
                    eventChanged = true;
                    break;
                }
            }

            // if event is not yet updated, then the contact hasn't been resolved yet -> add it to contacts
            if (!eventChanged) {

                contacts << newContact;
                eventChanged = true;
            }
        }

        else {

            qWarning() << "unknown contact change type???";
            break;
        }

        if (eventChanged) {

            // save the modified list back to the event
            event->setContacts(contacts);

            emitDataChanged(row, parent->child(row));
        }

        // dig down to children
        if (parent->child(row)->childCount()) {
            changeContactsRecursive(changeType,
                                    contactId,
                                    contactName,
                                    contactAddresses,
                                    parent->child(row));
        }
    }
}

void EventModelPrivate::slotContactUpdated(quint32 localId,
                                           const QString &contactName,
                                           const QList<ContactAddress> &contactAddresses)
{
    Event::Contact contact(localId, contactName);

    // (local id, remote id) -> (contact id, name)
    QMap<QPair<QString,QString>, QList<Event::Contact> >::iterator cacheIt = contactCache.begin();
    for ( ; cacheIt != contactCache.end(); ++cacheIt) {
        const QPair<QString, QString> &uidPair(cacheIt.key());
        QList<Event::Contact> &contacts(cacheIt.value());

        if (ContactListener::addressMatchesList(uidPair.first,  // local id
                                                uidPair.second, // remote id
                                                contactAddresses)) {
            QList<Event::Contact>::iterator it = contacts.begin(), end = contacts.end();
            for ( ; it != end; ++it) {
                if ((*it).first == contact.first) {
                    // change name for existing contact
                    (*it).second = contact.second;
                    break;
                }
            }
            if (it == end) {
                // add new contact to key
                contacts.append(contact);
            }
        }
        // address not found, but we've got the contact in the cache
        // -> contact was updated -> address removed
        else if (contacts.contains(contact)) {
            // delete our record since the address doesn't match anymore
            contacts.removeOne(contact);
            if (contacts.isEmpty()) {
                contactCache.erase(cacheIt);
            }
            break;
        }
    }

    QList<QPair<QString, QString> > uidPairs;

    bool hasAddressType[3] = { false, false, false };
    foreach (const ContactAddress &address, contactAddresses) {
        Q_ASSERT((address.type >= ContactListener::IMAccountType) && (address.type <= ContactListener::EmailAddressType));

        hasAddressType[address.type - 1] = true;
        uidPairs.append(address.uidPair());
    }

    QSet<quint32> * const typeSet[3] = { &imContacts, &phoneContacts, &emailContacts };
    for (int i = 0; i < 3; ++i) {
        if (hasAddressType[i]) {
            typeSet[i]->insert(localId);
        } else {
            typeSet[i]->remove(localId);
        }
    }

    changeContactsRecursive(ContactUpdated, localId, contactName, uidPairs, eventRootItem);
}

void EventModelPrivate::slotContactRemoved(quint32 localId)
{
    QList<QPair<QString,QString> > contactAddresses;

    QMap<QPair<QString,QString>, QList<Event::Contact> >::iterator cacheIt = contactCache.begin();
    while (cacheIt != contactCache.end()) {
        QList<Event::Contact> &contacts(cacheIt.value());

        QList<Event::Contact>::iterator it = contacts.begin();
        while (it != contacts.end()) {
            if (static_cast<quint32>((*it).first) == localId) {
                contactAddresses.append(cacheIt.key());
                it = contacts.erase(it);
            } else {
                ++it;
            }
        }

        if (contacts.isEmpty()) {
            cacheIt = contactCache.erase(cacheIt);
        } else {
            ++cacheIt;
        }
    }

    QSet<quint32> * const typeSet[3] = { &imContacts, &phoneContacts, &emailContacts };
    for (int i = 0; i < 3; ++i) {
        typeSet[i]->remove(localId);
    }

    changeContactsRecursive(ContactRemoved,
                            localId,
                            QString(), // contactName
                            contactAddresses,
                            eventRootItem);
}

void EventModelPrivate::slotContactUnknown(const QPair<QString, QString> &address)
{
    Q_UNUSED(address)
}

DatabaseIO* EventModelPrivate::database()
{
    return DatabaseIO::instance();
}

bool EventModelPrivate::setContactFromCache(CommHistory::Event &event)
{
    QMap<QPair<QString,QString>, QList<Event::Contact> >::Iterator it = contactCache.find(qMakePair(event.localUid(), event.remoteUid()));
    if (it != contactCache.end()) {
        if (!it.value().isEmpty()) {
            event.setContacts(it.value());
            return true;
        }
    } else {
        // Insert an empty item into the cache to prevent duplicate requests
        contactCache.insert(qMakePair(event.localUid(), event.remoteUid()), QList<Event::Contact>());

        startContactListening();
        if (contactListener)
            contactListener->resolveContact(event.localUid(), event.remoteUid());
    }

    return false;
}

void EventModelPrivate::startContactListening()
{
    if (contactChangesEnabled && !contactListener) {
        contactListener = ContactListener::instance();
        connect(contactListener.data(),
                SIGNAL(contactUpdated(quint32, const QString&, const QList<ContactAddress>&)),
                this,
                SLOT(slotContactUpdated(quint32, const QString&, const QList<ContactAddress>&)),
                Qt::UniqueConnection);
        connect(contactListener.data(),
                SIGNAL(contactRemoved(quint32)),
                this,
                SLOT(slotContactRemoved(quint32)),
                Qt::UniqueConnection);
        connect(contactListener.data(),
                SIGNAL(contactUnknown(const QPair<QString, QString>&)),
                this,
                SLOT(slotContactUnknown(const QPair<QString, QString>&)),
                Qt::UniqueConnection);
    }
}

bool EventModelPrivate::contactHasAddress(int types, quint32 localId) const
{
    if ((types & RecentContactsModel::PhoneNumberRequired) && phoneContacts.contains(localId))
        return true;
    if ((types & RecentContactsModel::EmailAddressRequired) && emailContacts.contains(localId))
        return true;
    if ((types & RecentContactsModel::AccountUriRequired) && imContacts.contains(localId))
        return true;
    return false;
}

void EventModelPrivate::emitDataChanged(int row, void *data)
{
    Q_Q(EventModel);

    const QModelIndex left(q->createIndex(row, 0, data));
    const QModelIndex right(q->createIndex(row, EventModel::NumberOfColumns - 1, data));
    emit q->dataChanged(left, right);
}

