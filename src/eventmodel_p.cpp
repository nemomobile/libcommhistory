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

using namespace CommHistory;

namespace {

bool initializeTypes()
{
    qRegisterMetaType<QList<CommHistory::Event> >();
    return true;
}

const int defaultChunkSize = 50;

}

bool eventmodel_p_initialized = initializeTypes();

EventModelPrivate::EventModelPrivate(EventModel *model)
        : addResolver(0)
        , receiveResolver(0)
        , isInTreeMode(false)
        , queryMode(EventModel::AsyncQuery)
        , chunkSize(defaultChunkSize)
        , firstChunkSize(0)
        , queryLimit(0)
        , queryOffset(0)
        , isReady(true)
        , threadCanFetchMore(false)
        , resolveContacts(false)
        , propertyMask(Event::allProperties())
        , bgThread(0)
{
    q_ptr = model;

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

    if (id < 0)
        return QModelIndex();

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

bool EventModelPrivate::executeQuery(QSqlQuery &query)
{
    DEBUG() << __PRETTY_FUNCTION__;

    isReady = false;

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    QList<Event> events;
    QList<int> extraPropertyIndicies;
    QList<int> hasPartsIndicies;
    while (query.next()) {
        Event e;
        bool extra = false, parts = false;
        DatabaseIOPrivate::readEventResult(query, e, extra, parts);
        if (extra)
            extraPropertyIndicies.append(events.size());
        if (parts)
            hasPartsIndicies.append(events.size());
        events.append(e);
    }
    query.finish();

    foreach (int i, extraPropertyIndicies)
        DatabaseIO::instance()->getEventExtraProperties(events[i]);
    foreach (int i, hasPartsIndicies)
        DatabaseIO::instance()->getMessageParts(events[i]);

    eventsReceivedSlot(0, events.size(), events);
    return true;
}

bool EventModelPrivate::fillModel(int start,
                                 int end,
                                 QList<CommHistory::Event> events)
{
    Q_UNUSED(start);
    Q_UNUSED(end);

    if (events.isEmpty())
        return false;

    Q_Q(EventModel);
    DEBUG() << __PRETTY_FUNCTION__ << ": read" << events.count() << "events";

    q->beginInsertRows(QModelIndex(), q->rowCount(), q->rowCount() + events.count() - 1);
    foreach (const Event &event, events) {
        eventRootItem->appendChild(new EventTreeItem(event, eventRootItem));
    }
    q->endInsertRows();

    modelUpdatedSlot(true);
    return true;
}

bool EventModelPrivate::fillModel(QList<CommHistory::Event> events)
{
    Q_Q(EventModel);

    QMutableListIterator<Event> i(events);
    while (i.hasNext()) {
        const Event &event = i.next();
        if (findEvent(event.id()).isValid()) {
            i.remove();
            continue;
        }
    }

    return fillModel(q->rowCount(), q->rowCount() + events.count() - 1, events);
}

void EventModelPrivate::clearEvents()
{
    DEBUG() << __PRETTY_FUNCTION__;
    delete eventRootItem;
    eventRootItem = new EventTreeItem(Event());
}

void EventModelPrivate::addToModel(const Event &event, bool sync)
{
    DEBUG() << Q_FUNC_INFO << event.toString();

    // Insert immediately if synchronous (and resolve contact later if necessary),
    // or if not resolving contacts
    if (sync || !resolveContacts) {
        prependEvents(QList<Event>() << event);
    }

    // Resolve contacts. If inserted above, the duplicate will be ignored
    if (resolveContacts) {
        if (!addResolver) {
            addResolver = new ContactResolver(this);
            connect(addResolver, SIGNAL(finished()), SLOT(addResolverFinished()));
        }

        pendingAdded.prepend(event);
        addResolver->add(event);
    }
}

void EventModelPrivate::addResolverFinished()
{
    QList<Event> added = pendingAdded;
    pendingAdded.clear();
    prependEvents(added);
}

void EventModelPrivate::prependEvents(QList<Event> events)
{
    Q_Q(EventModel);

    // Replace exact duplicates instead of inserting. This is a workaround
    // for the sync mode in addToModel.
    for (int i = 0; i < events.size(); i++) {
        for (int j = 0; j < eventRootItem->childCount(); j++) {
            if (eventRootItem->eventAt(j) == events[i]) {
                eventRootItem->child(j)->setEvent(events[i]);
                emitDataChanged(j, eventRootItem->child(j));
                events.removeAt(i);
                i--;
                break;
            }
        }
    }

    if (events.isEmpty())
        return;

    q->beginInsertRows(QModelIndex(), 0, events.size() - 1);
    for (int i = events.size() - 1; i >= 0; i--) {
        eventRootItem->prependChild(new EventTreeItem(events[i], eventRootItem));
    }
    q->endInsertRows();
}

void EventModelPrivate::modifyInModel(Event &event)
{
    Q_Q(EventModel);
    DEBUG() << __PRETTY_FUNCTION__ << event.id();

    QModelIndex index = findEvent(event.id());
    if (index.isValid()) {
        EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
        Event oldEvent = item->event();
        quint32 oldTimeT = oldEvent.endTimeT();
        oldEvent.copyValidProperties(event);
        item->setEvent(oldEvent);

        // move event if endTime has changed
        if (index.row() > 0 && oldTimeT < event.endTimeT()) {
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

    if (events.isEmpty()) {
        // Empty results are still "ready"
        modelUpdatedSlot(true);
        return;
    }

    // Contact resolution is not allowed in synchronous query mode
    if (resolveContacts && queryMode != EventModel::SyncQuery) {
        if (!receiveResolver) {
            receiveResolver = new ContactResolver(this);
            connect(receiveResolver, SIGNAL(finished()), SLOT(receiveResolverFinished()));
        }

        pendingReceived.append(events);
        receiveResolver->add(events);
    } else {
        fillModel(start, end, events);
    }
}

void EventModelPrivate::receiveResolverFinished()
{
    QList<Event> received = pendingReceived;
    pendingReceived.clear();
    fillModel(received);
}

void EventModelPrivate::modelUpdatedSlot(bool successful)
{
    DEBUG() << __PRETTY_FUNCTION__;

    isReady = true;
    emit modelReady(successful);
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
    return threadCanFetchMore;
}

void EventModelPrivate::recipientsChangedRecursive(const QSet<Recipient> &recipients, EventTreeItem *parent)
{
    for (int row = 0; row < parent->childCount(); row++) {
        const Event &event(parent->eventAt(row));
        EventTreeItem *child = parent->child(row);
        if (event.recipients().intersects(recipients)) {
            // XXX coalesce
            // XXX role dataChanged signal
            emitDataChanged(row, child);
        }
        if (child->childCount())
            recipientsChangedRecursive(recipients, child);
    }
}

void EventModelPrivate::slotContactInfoChanged(const RecipientList &recipients)
{
    QSet<Recipient> changed = QSet<Recipient>::fromList(recipients.recipients());
    recipientsChangedRecursive(changed, eventRootItem);
}

void EventModelPrivate::slotContactChanged(const RecipientList &recipients)
{
    QSet<Recipient> changed = QSet<Recipient>::fromList(recipients.recipients());
    recipientsChangedRecursive(changed, eventRootItem);
}

DatabaseIO* EventModelPrivate::database()
{
    return DatabaseIO::instance();
}

void EventModelPrivate::setResolveContacts(bool enabled)
{
    if (resolveContacts == enabled)
        return;
    resolveContacts = enabled;

    if (resolveContacts && !contactListener) {
        contactListener = ContactListener::instance();
        connect(contactListener.data(),
                SIGNAL(contactInfoChanged(RecipientList)),
                SLOT(slotContactInfoChanged(RecipientList)));
        connect(contactListener.data(),
                SIGNAL(contactChanged(RecipientList)),
                SLOT(slotContactChanged(RecipientList)));
    } else if (!resolveContacts && contactListener) {
        disconnect(contactListener.data(), 0, this, 0);
        contactListener.clear();

        delete receiveResolver;
        receiveResolver = 0;
    }
}

void EventModelPrivate::emitDataChanged(int row, void *data)
{
    Q_Q(EventModel);

    const QModelIndex left(q->createIndex(row, 0, data));
    const QModelIndex right(q->createIndex(row, EventModel::NumberOfColumns - 1, data));
    emit q->dataChanged(left, right);
}

