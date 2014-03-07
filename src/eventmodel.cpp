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

#include "databaseio.h"
#include "eventmodel.h"
#include "eventmodel_p.h"
#include "adaptor.h"
#include "event.h"
#include "eventtreeitem.h"
#include "debug.h"

using namespace CommHistory;

EventModel::EventModel(QObject *parent)
        : QAbstractItemModel(parent)
        , d_ptr(new EventModelPrivate(this))
{
    connect(d_ptr, SIGNAL(modelReady(bool)), this, SIGNAL(modelReady(bool)));
    connect(d_ptr, SIGNAL(eventsCommitted(QList<CommHistory::Event>,bool)),
            this, SIGNAL(eventsCommitted(QList<CommHistory::Event>,bool)));
}

EventModel::EventModel(EventModelPrivate &dd, QObject *parent)
        : QAbstractItemModel(parent), d_ptr(&dd)
{
    connect(d_ptr, SIGNAL(modelReady(bool)), this, SIGNAL(modelReady(bool)));
    connect(d_ptr, SIGNAL(eventsCommitted(QList<CommHistory::Event>,bool)),
            this, SIGNAL(eventsCommitted(QList<CommHistory::Event>,bool)));
}

EventModel::~EventModel()
{
    delete d_ptr;
}

QHash<int, QByteArray> EventModel::roleNames() const
{
    QHash<int,QByteArray> roles;
    roles[BaseRole + EventId] = "eventId";
    roles[BaseRole + EventType] = "eventType";
    roles[BaseRole + StartTime] = "startTime";
    roles[BaseRole + EndTime] = "endTime";
    roles[BaseRole + Direction] = "direction";
    roles[BaseRole + IsDraft] = "isDraft";
    roles[BaseRole + IsRead] = "isRead";
    roles[BaseRole + IsMissedCall] = "isMissedCall";
    roles[BaseRole + Status] = "status";
    roles[BaseRole + BytesReceived] = "bytesReceived";
    roles[BaseRole + LocalUid] = "localUid";
    roles[BaseRole + RemoteUid] = "remoteUid";
    roles[BaseRole + FreeText] = "freeText";
    roles[BaseRole + GroupId] = "groupId";
    roles[BaseRole + MessageToken] = "messageToken";
    roles[BaseRole + LastModified] = "lastModified";
    roles[BaseRole + EventCount] = "eventCount";
    roles[BaseRole + FromVCardFileName] = "fromVCardFileName";
    roles[BaseRole + FromVCardLabel] = "fromVCardLabel";
    roles[ContactIdsRole] = "contactIds";
    roles[ContactNamesRole] = "contactNames";
    return roles;
}

void EventModel::setPropertyMask(const Event::PropertySet &properties)
{
    Q_D(EventModel);
    d->propertyMask = properties;
}

bool EventModel::isTree() const
{
    Q_D(const EventModel);
    return d->isInTreeMode;
}

EventModel::QueryMode EventModel::queryMode() const
{
    Q_D(const EventModel);
    return d->queryMode;
}

uint EventModel::chunkSize() const
{
    Q_D(const EventModel);
    return d->chunkSize;
}

uint EventModel::firstChunkSize() const
{
    Q_D(const EventModel);
    return d->firstChunkSize;
}

int EventModel::limit() const
{
    Q_D(const EventModel);
    return d->queryLimit;
}

int EventModel::offset() const
{
    Q_D(const EventModel);
    return d->queryOffset;
}

bool EventModel::isReady() const
{
    Q_D(const EventModel);
    return d->isReady;
}

QModelIndex EventModel::parent(const QModelIndex &index) const
{
    Q_D(const EventModel);
    if (!index.isValid()) {
        return QModelIndex();
    }

    EventTreeItem *childItem = static_cast<EventTreeItem *>(index.internalPointer());
    EventTreeItem *parentItem = childItem->parent();

    if (!parentItem || parentItem == d->eventRootItem) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

bool EventModel::hasChildren(const QModelIndex &parent) const
{
    Q_D(const EventModel);
    EventTreeItem *item;
    if (!parent.isValid()) {
        item = d->eventRootItem;
    } else {
        item = static_cast<EventTreeItem *>(parent.internalPointer());
    }
    return (item && item->childCount() > 0);
}

QModelIndex EventModel::index(int row, int column,
                                        const QModelIndex &parent) const
{
    Q_D(const EventModel);
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    EventTreeItem *parentItem;
    if (!parent.isValid()) {
        parentItem = d->eventRootItem;
    } else {
        parentItem = static_cast<EventTreeItem *>(parent.internalPointer());
    }

    EventTreeItem *childItem = parentItem->child(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    }

    return QModelIndex();
}

int EventModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const EventModel);
    if (parent.column() > 0) {
        return 0;
    }

    EventTreeItem *parentItem;
    if (!parent.isValid()) {
        parentItem = d->eventRootItem;
    } else {
        parentItem = static_cast<EventTreeItem *>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int EventModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return NumberOfColumns;
}

static QList<int> contactIds(const QList<Event::Contact> &contacts)
{
    QList<int> re;
    re.reserve(contacts.size());
    foreach (const Event::Contact &c, contacts)
        re.append(c.first);
    return re;
}

static QList<QString> contactNames(const QList<Event::Contact> &contacts)
{
    QList<QString> re;
    re.reserve(contacts.size());
    foreach (const Event::Contact &c, contacts)
        re.append(c.second);
    return re;
}

QVariant EventModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
    Event &event = item->event();

    switch (role) {
        case EventRole:
            return QVariant::fromValue(event);
        case ContactIdsRole:
            return QVariant::fromValue(contactIds(event.contacts()));
        case ContactNamesRole:
            return QVariant::fromValue(contactNames(event.contacts()));
        default:
            break;
    }

    int column = index.column();
    if (role >= BaseRole) {
        column = role - BaseRole;
        role = Qt::DisplayRole;
    }

    QVariant var;
    switch (column) {
        case EventId:
            var = QVariant::fromValue(event.id());
            break;
        case EventType:
            var = QVariant::fromValue((int)(event.type()));
            break;
        case StartTime:
            var = QVariant::fromValue(event.startTime());
            break;
        case EndTime:
            var = QVariant::fromValue(event.endTime());
            break;
        case Direction:
            var = QVariant::fromValue((int)(event.direction()));
            break;
        case IsDraft:
            var = QVariant::fromValue(event.isDraft());
            break;
        case IsRead:
            var = QVariant::fromValue(event.isRead());
            break;
        case IsMissedCall:
            var = QVariant::fromValue(event.isMissedCall());
            break;
        case Status:
            var = QVariant::fromValue((int)(event.status()));
            break;
        case BytesReceived:
            var = QVariant::fromValue(event.bytesReceived());
            break;
        case LocalUid:
            var = QVariant::fromValue(event.localUid());
            break;
        case RemoteUid:
            var = QVariant::fromValue(event.remoteUid());
            break;
        case Contacts:
            var = QVariant::fromValue(event.contacts());
            break;
        case FreeText:
            var = QVariant::fromValue(event.freeText());
            break;
        case GroupId:
            var = QVariant::fromValue(event.groupId());
            break;
        case MessageToken:
            var = QVariant::fromValue(event.messageToken());
            break;
        case LastModified:
            var = QVariant::fromValue(event.lastModified());
            break;
        case EventCount:
            var = QVariant::fromValue(event.eventCount());
            break;
        case FromVCardFileName:
            var = QVariant::fromValue(event.fromVCardFileName());
            break;
        case FromVCardLabel:
            var = QVariant::fromValue(event.fromVCardLabel());
            break;
        default:
            DEBUG() << __PRETTY_FUNCTION__ << ": invalid column id??" << column;
            var = QVariant();
            break;
    }

    return var;
}

Event EventModel::event(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Event();
    }

    EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
    return item->event();
}

QModelIndex EventModel::findEvent(int id) const
{
    Q_D(const EventModel);

    return d->findEvent(id);
}

QVariant EventModel::headerData(int section,
                                      Qt::Orientation orientation,
                                      int role) const
{
    QVariant var;
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        QString name;
        switch (section) {
            case EventId:
                name = QLatin1String("id");
                break;
            case EventType:
                name = QLatin1String("event_type");
                break;
            case StartTime:
                name = QLatin1String("start_time");
                break;
            case EndTime:
                name = QLatin1String("end_time");
                break;
            case Direction:
                name = QLatin1String("direction");
                break;
            case IsDraft:
                name = QLatin1String("is_draft");
                break;
            case IsRead:
                name = QLatin1String("is_read");
                break;
            case IsMissedCall:
                name = QLatin1String("is_missed_call");
                break;
            case Status:
                name = QLatin1String("status");
                break;
            case BytesReceived:
                name = QLatin1String("bytes_received");
                break;
            case LocalUid:
                name = QLatin1String("local_uid");
                break;
            case RemoteUid:
                name = QLatin1String("remote_uid");
                break;
            case Contacts:
                name = QLatin1String("contacts");
                break;
            case FreeText:
                name = QLatin1String("free_text");
                break;
            case GroupId:
                name = QLatin1String("group_id");
                break;
            case MessageToken:
                name = QLatin1String("message_token");
                break;
            case LastModified:
                name = QLatin1String("last_modified");
                break;
            case EventCount:
                name = QLatin1String("event_count");
                break;
            case FromVCardFileName:
                name = QLatin1String("vcard_filename");
                break;
            case FromVCardLabel:
                name = QLatin1String("vcard_label");
                break;
            default:
                break;
        }
        var = QVariant::fromValue(name);
    }

    return var;
}

void EventModel::setTreeMode(bool isTree)
{
    Q_D(EventModel);
    d->isInTreeMode = isTree;
}

void EventModel::setQueryMode(QueryMode mode)
{
    Q_D(EventModel);
    if (d->queryMode == mode)
        return;

    d->queryMode = mode;
    if (d->queryMode == SyncQuery && d->resolveContacts)
        qWarning() << "EventMode does not support contact resolution for synchronous models. Contacts will not be resolved.";
}

void EventModel::setChunkSize(uint size)
{
    Q_D(EventModel);
    d->chunkSize = size;
}

void EventModel::setFirstChunkSize(uint size)
{
    Q_D(EventModel);
    d->firstChunkSize = size;
}

void EventModel::setLimit(int limit)
{
    Q_D(EventModel);
    d->queryLimit = limit;
}

void EventModel::setOffset(int offset)
{
    Q_D(EventModel);
    d->queryOffset = offset;
}

bool EventModel::resolveContacts() const
{
    Q_D(const EventModel);
    return d->resolveContacts;
}

void EventModel::setResolveContacts(bool enabled)
{
    Q_D(EventModel);
    if (enabled == d->resolveContacts)
        return;

    if (d->queryMode == SyncQuery && d->resolveContacts)
        qWarning() << "EventMode does not support contact resolution for synchronous models. Contacts will not be resolved.";

    d->setResolveContacts(enabled);
}

bool EventModel::addEvent(Event &event, bool toModelOnly)
{
    QList<Event> list;
    list << event;
    bool ok = addEvents(list, toModelOnly);
    event = list.first();
    return ok;
}

bool EventModel::addEvents(QList<Event> &events, bool toModelOnly)
{
    Q_D(EventModel);

    if (!toModelOnly) {
        // Insert the events into the database
        if (!d->database()->transaction())
            return false;

        // Cannot be foreach, because addEvent modifies the events with their new ID
        for (int i = 0; i < events.size(); i++) {
            if (!d->database()->addEvent(events[i])) {
                d->database()->rollback();
                return false;
            }
        }

        if (!d->database()->commit())
            return false;
    } else {
        // Set ids to have valid events
        int firstReservedId;
        if (!d->database()->reserveEventIds(events.size(), &firstReservedId))
            return false;
        for (int i = 0; i < events.size(); i++)
            events[i].setId(firstReservedId + i);
    }

    foreach (Event event, events) {
        if (d->acceptsEvent(event)) {
            // Add synchronously to preserve current API guarantees
            // Contacts will be via dataChanged. Fix when models are async.
            d->addToModel(event, true);
        }
    }

    emit d->eventsAdded(events);

    if (!toModelOnly)
        emit d->eventsCommitted(events, true);

    return true;
}

bool EventModel::modifyEvent(Event &event)
{
    QList<Event> list;
    list << event;
    bool ok = modifyEvents(list);
    event = list.first();
    return ok;
}

bool EventModel::modifyEvents(QList<Event> &events)
{
    Q_D(EventModel);

    if (!d->database()->transaction())
        return false;

    QList<int> modifiedGroups;
    for (QList<Event>::Iterator it = events.begin(); it != events.end(); it++) {
        Event &event = *it;
        if (event.id() == -1) {
            qWarning() << Q_FUNC_INFO << "Event id not set";
            d->database()->rollback();
            return false;
        }

        if (event.lastModified() == QDateTime::fromTime_t(0))
             event.setLastModified(QDateTime::currentDateTime());

        if (!d->database()->modifyEvent(event)) {
            d->database()->rollback();
            return false;
        }

        if (event.isValid()
            && event.groupId() != -1
            && !event.isDraft()
            && !modifiedGroups.contains(event.groupId())) {
            modifiedGroups.append(event.groupId());
        }
    }

    if (!d->database()->commit())
        return false;

    emit d->eventsUpdated(events);
    if (!modifiedGroups.isEmpty())
        emit d->groupsUpdated(modifiedGroups);
    emit d->eventsCommitted(events, true);

    return true;
}

bool EventModel::deleteEvent(int id)
{
    Q_D(EventModel);
    DEBUG() << __FUNCTION__ << ":" << id;

    QModelIndex index = d->findEvent(id);
    Event event;
    if (index.isValid())
        event = this->event(index);
    else if (!d->database()->getEvent(id, event))
        return false;

    deleteEvent(event);
    return true;
}

bool EventModel::deleteEvent(Event &event)
{
    Q_D(EventModel);
    DEBUG() << __FUNCTION__ << ":" << event.id();

    if (!event.isValid()) {
        qWarning() << __FUNCTION__ << "Invalid event";
        return false;
    }

    if (!d->database()->transaction())
        return false;

    if (!d->database()->deleteEvent(event, d->bgThread)) {
        d->database()->rollback();
        return false;
    }

    bool groupUpdated = false, groupDeleted = false;
    if (event.groupId() != -1 && !event.isDraft()) {
        int total;
        if (!d->database()->totalEventsInGroup(event.groupId(), total)) {
            d->database()->rollback();
            return false;
        }

        if (total == 0) {
            DEBUG() << __FUNCTION__ << ": deleting empty group";
            if (!d->database()->deleteGroup(event.groupId())) {
                d->database()->rollback();
                return false;
            } else
                groupDeleted = true;
        } else
            groupUpdated = true;
    }

    if (!d->database()->commit())
        return false;

    emit d->eventDeleted(event.id());

    if (groupDeleted)
        emit d->groupsDeleted(QList<int>() << event.groupId());
    else if (groupUpdated)
        emit d->groupsUpdated(QList<int>() << event.groupId());

    emit d->eventsCommitted(QList<Event>() << event, true);

    return true;
}

bool EventModel::moveEvent(Event &event, int groupId)
{
    Q_D(EventModel);
    DEBUG() << __FUNCTION__ << ":" << event.id();

    if (!event.isValid()) {
        qWarning() << __FUNCTION__ << "Invalid event";
        return false;
    }

    if (event.groupId() == groupId)
        return true;

    // DatabaseIO::moveEvent changes groupId
    int oldGroupId = event.groupId();

    if (!d->database()->transaction())
        return false;

    if (!d->database()->moveEvent(event, groupId)) {
        d->database()->rollback();
        return false;
    }

    int groupDeleted = -1;
    // update or delete old group
    if (oldGroupId != -1 && !event.isDraft()) {
        int total;
        if (!d->database()->totalEventsInGroup(oldGroupId, total)) {
            d->database()->rollback();
            return false;
        }

        if (total == 0) {
            DEBUG() << __FUNCTION__ << ": deleting empty group";
            if (!d->database()->deleteGroup(oldGroupId)) {
                qWarning() << Q_FUNC_INFO << "error deleting empty group" ;
                d->database()->rollback();
                return false;
            } else
                groupDeleted = oldGroupId;
        }
    }

    if (!d->database()->commit())
        return false;

    emit d->eventDeleted(event.id());
    if (groupDeleted != -1)
        emit d->groupsDeleted(QList<int>() << groupDeleted);
    else if (oldGroupId != -1)
        emit d->groupsUpdated(QList<int>() << oldGroupId);

    emit d->groupsUpdated(QList<int>() << groupId);
    emit d->eventsAdded(QList<Event>() << event);
    emit d->eventsCommitted(QList<Event>() << event, true);

    return true;
}

bool EventModel::modifyEventsInGroup(QList<Event> &events, Group group)
{
    Q_D(EventModel);

    if (events.isEmpty())
        return true;

    if (!d->database()->transaction())
        return false;

    for (QList<Event>::Iterator it = events.begin(); it != events.end(); it++) {
        Event &event = *it;
        if (event.id() == -1) {
            qWarning() << Q_FUNC_INFO << "Event id not set";
            d->database()->rollback();
            return false;
        }

        if (event.lastModified() == QDateTime::fromTime_t(0)) {
             event.setLastModified(QDateTime::currentDateTime());
        }

        if (!d->database()->modifyEvent(event)) {
            d->database()->rollback();
            return false;
        }
        if (group.lastEventId() == event.id()
            || event.endTime() > group.endTime()) {
            Event::PropertySet modified;
            if (group.lastEventId() == event.id()) {
                modified = event.modifiedProperties();
            } else {
                group.setLastEventId(event.id());
                modified = Event::allProperties();
            }

            if (modified.contains(Event::Status))
                group.setLastEventStatus(event.status());
            group.setLastEventType(event.type());
            //text might be changed in case of MMS
            if (modified.contains(Event::FreeText)
                || modified.contains(Event::Subject)) {
                if(event.type() == Event::MMSEvent) {
                    group.setLastMessageText(event.subject().isEmpty() ? event.freeText() : event.subject());
                } else {
                    group.setLastMessageText(event.freeText());
                }
            }
            if (modified.contains(Event::IsRead)) {
                if (event.isRead())
                    group.setUnreadMessages(qMax(group.unreadMessages() - 1, 0));
                else
                    group.setUnreadMessages(group.unreadMessages() + 1);
            }
            if (modified.contains(Event::FromVCardFileName)
                || modified.contains(Event::FromVCardLabel)) {
                group.setLastVCardFileName(event.fromVCardFileName());
                group.setLastVCardLabel(event.fromVCardLabel());
            }
            if (modified.contains(Event::StartTime))
                group.setStartTime(event.startTime());
            if (modified.contains(Event::EndTime))
                group.setEndTime(event.endTime());
        }
    }

    if (!d->database()->commit())
        return false;

    emit d->eventsUpdated(events);
    emit d->groupsUpdatedFull(QList<Group>() << group);
    emit d->eventsCommitted(events, true);
    return true;
}

bool EventModel::canFetchMore(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    Q_D(const EventModel);

    return d->canFetchMore();
}

void EventModel::fetchMore(const QModelIndex &parent)
{
    Q_UNUSED(parent);
}

void EventModel::setBackgroundThread(QThread *thread)
{
    Q_D(EventModel);

    d->bgThread = thread;
    DEBUG() << Q_FUNC_INFO << thread;
}

QThread* EventModel::backgroundThread()
{
    Q_D(EventModel);

    return d->bgThread;
}

DatabaseIO& EventModel::databaseIO()
{
    Q_D(EventModel);

    return *d->database();
}
