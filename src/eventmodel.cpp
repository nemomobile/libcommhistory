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
#include <QtTracker/Tracker>
#include <QtDebug>
#include <QUrl>

#include "trackerio.h"
#include "eventmodel.h"
#include "eventmodel_p.h"
#include "adaptor.h"
#include "event.h"
#include "eventtreeitem.h"
#include "queryrunner.h"
#include "committingtransaction.h"

using namespace SopranoLive;

using namespace CommHistory;

#define MAX_ADD_EVENTS_SIZE 25

EventModel::EventModel(QObject *parent)
        : QAbstractItemModel(parent)
        , d_ptr(new EventModelPrivate(this))
{
    connect(d_ptr, SIGNAL(modelReady()), this, SIGNAL(modelReady()));
    connect(d_ptr, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,bool)),
            this, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,bool)));
}

EventModel::EventModel(EventModelPrivate &dd, QObject *parent)
        : QAbstractItemModel(parent), d_ptr(&dd)
{
    connect(d_ptr, SIGNAL(modelReady()), this, SIGNAL(modelReady()));
    connect(d_ptr, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,bool)),
            this, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,bool)));
}

EventModel::~EventModel()
{
    delete d_ptr;
}

QSqlError EventModel::lastError() const
{
    Q_D(const EventModel);
    return d->lastError;
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

bool EventModel::syncMode() const
{
    Q_D(const EventModel);
    return d->syncOnCommit;
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

QVariant EventModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(role);

    if (!index.isValid()) {
        return QVariant();
    }

    EventTreeItem *item = static_cast<EventTreeItem *>(index.internalPointer());
    Event &event = item->event();

    if (role == Qt::UserRole) {
        return QVariant::fromValue(event);
    }

    QVariant var;
    switch (index.column()) {
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
        case BytesSent:
            var = QVariant::fromValue(event.bytesSent());
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
        case ContactId:
            var = QVariant::fromValue(event.contactId());
            break;
        case ContactName:
            var = QVariant::fromValue(event.contactName());
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
        case Encoding:
            var = QVariant::fromValue(event.encoding());
            break;
        case Charset:
            var = QVariant::fromValue(event.characterSet());
            break;
        case Language:
            var = QVariant::fromValue(event.language());
            break;
        case IsDeleted:
            var = QVariant::fromValue(event.isDeleted());
            break;
        default:
            qDebug() << __PRETTY_FUNCTION__ << ": invalid column id??" << index.column();
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
            case BytesSent:
                name = QLatin1String("bytes_sent");
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
            case ContactId:
                name = QLatin1String("contact_id");
                break;
            case ContactName:
                name = QLatin1String("contact_name");
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
            case Encoding:
                name = QLatin1String("encoding");
                break;
            case Charset:
                name = QLatin1String("charset");
                break;
            case Language:
                name = QLatin1String("language");
                break;
            case IsDeleted:
                name = QLatin1String("is_deleted");
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
    d->queryMode = mode;
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

void EventModel::setSyncMode(bool mode)
{
    Q_D(EventModel);
    d->syncOnCommit = mode;
}

void EventModel::enableContactChanges(bool enabled)
{
    Q_D(EventModel);
    d->contactChangesEnabled = enabled;
}

bool EventModel::addEvent(Event &event, bool toModelOnly)
{
    Q_D(EventModel);

    // if event needs to be added to model only,
    // then don't use anything tracker related.
    // add it to model and send signal.
    if ( toModelOnly ) {
        // set id to have a valid event
        event.setId( d->tracker()->nextEventId() );

        if (d->acceptsEvent(event)) {
            d->addToModel(event);
        }
        emit d->eventsAdded(QList<Event>() << event);
    }
    // otherwise execute proper db operations.
    else {
        d->tracker()->transaction(d->syncOnCommit);
        if (!d->doAddEvent(event)) {
            d->tracker()->rollback();
            return false;
        }

        if (d->acceptsEvent(event)) {
            d->addToModel(event);
        }

        d->commitTransaction(QList<Event>() << event);
        emit d->eventsAdded(QList<Event>() << event);
    }
    return true;
}

bool EventModel::addEvents(QList<Event> &events, bool toModelOnly)
{
    Q_D(EventModel);

    QMutableListIterator<Event> i(events);
    QList<Event> added;

    // if event needs to be added to model only,
    // then don't use anything tracker related.
    // add it to model and send signal.
    if ( toModelOnly ) {
        while (i.hasNext()) {
            while (added.size() < MAX_ADD_EVENTS_SIZE && i.hasNext()) {
                Event &event = i.next();

                // set id to have a valid event
                event.setId( d->tracker()->nextEventId() );

                if (d->acceptsEvent(event)) {
                    d->addToModel(event);
                }

                added.append(event);
            }

            emit d->eventsAdded(added);
            added.clear();
        }
    }
    // otherwise execute proper db operations.
    else {
        while (i.hasNext()) {
            d->tracker()->transaction(d->syncOnCommit);

            while (added.size() < MAX_ADD_EVENTS_SIZE && i.hasNext()) {
                Event &event = i.next();

                if (!d->doAddEvent(event)) {
                    d->tracker()->rollback();
                    return false;
                }

                if (d->acceptsEvent(event)) {
                    d->addToModel(event);
                }

                added.append(event);
            }

            d->commitTransaction(added);
            emit d->eventsAdded(added);
            added.clear();
        }
    }

    return true;
}

bool EventModel::modifyEvent(Event &event)
{
    Q_D(EventModel);

    d->tracker()->transaction(d->syncOnCommit);

    if (event.id() == -1) {
        d->lastError = QSqlError();
        d->lastError.setType(QSqlError::TransactionError);
        d->lastError.setDatabaseText(QLatin1String("Event id not set"));
        qWarning() << Q_FUNC_INFO << ":" << d->lastError;
        d->tracker()->rollback();
        return false;
    }

    if (event.lastModified() == QDateTime::fromTime_t(0)) {
         event.setLastModified(QDateTime::currentDateTime());
    }

    if (!d->tracker()->modifyEvent(event)) {
        d->lastError = d->tracker()->lastError();
        if (d->lastError.isValid())
            qWarning() << __FUNCTION__ << ":" << d->lastError;

        d->tracker()->rollback();
        return false;
    }

    QList<Event> events;
    events << event;
    CommittingTransaction &t = d->commitTransaction(events);

    // add model update signals to emit on transaction commit
    t.addSignal("eventsUpdated",
                Q_ARG(QList<CommHistory::Event>, events));
    if (event.id() == -1
        && event.groupId() != -1
        && !event.isDraft())
        t.addSignal("groupsUpdated",
                    Q_ARG(QList<int>, QList<int>() << event.groupId()));

    qDebug() << __FUNCTION__ << ": updated event" << event.id();

    return true;
}

bool EventModel::modifyEvents(QList<Event> &events)
{
    Q_D(EventModel);

    d->tracker()->transaction(d->syncOnCommit);
    QList<int> modifiedGroups;
    QMutableListIterator<Event> i(events);
    while (i.hasNext()) {
        Event event = i.next();
        if (event.id() == -1) {
            d->lastError = QSqlError();
            d->lastError.setType(QSqlError::TransactionError);
            d->lastError.setDatabaseText(QLatin1String("Event id not set"));
            qWarning() << Q_FUNC_INFO << ":" << d->lastError;
            d->tracker()->rollback();
            return false;
        }

        if (event.lastModified() == QDateTime::fromTime_t(0)) {
             event.setLastModified(QDateTime::currentDateTime());
        }

        if (!d->tracker()->modifyEvent(event)) {
            d->lastError = d->tracker()->lastError();
            if (d->lastError.isValid())
                qWarning() << __FUNCTION__ << ":" << d->lastError;

            d->tracker()->rollback();
            return false;
        }

        if (event.id() == -1
            && event.groupId() != -1
            && !event.isDraft()
            && !modifiedGroups.contains(event.groupId())) {
            modifiedGroups.append(event.groupId());
        }
    }

    CommittingTransaction &t = d->commitTransaction(events);
    t.addSignal("eventsUpdated", Q_ARG(QList<CommHistory::Event>, events));
    if (!modifiedGroups.isEmpty())
        t.addSignal("groupsUpdated",
                    Q_ARG(QList<int>, modifiedGroups));

    return true;
}

bool EventModel::deleteEvent(int id)
{
    Q_D(EventModel);
    qDebug() << __FUNCTION__ << ":" << id;

    d->tracker()->transaction(d->syncOnCommit);

    Event event;
    if (!d->doDeleteEvent(id, event)) {
        d->tracker()->rollback();
        return false;
    }

    // update or delete group
    const char *groupSignal = 0;
    if (event.groupId() != -1 && !event.isDraft()) {
        int total;
        if (!d->tracker()->totalEventsInGroup(event.groupId(), total)) {
            d->lastError = d->tracker()->lastError();
            if (d->lastError.isValid())
                qWarning() << Q_FUNC_INFO << d->lastError;
            d->tracker()->rollback();
            return false;
        }
        if (total == 1) {
            qDebug() << __FUNCTION__ << ": deleting empty group";
            if  (!d->tracker()->deleteGroup(event.groupId(), false)) {
                d->lastError = d->tracker()->lastError();
                if (d->lastError.isValid())
                    qWarning() << Q_FUNC_INFO << d->lastError;
                d->tracker()->rollback();
                return false;
            } else {
                groupSignal = "groupsDeleted";
            }
        } else {
            groupSignal = "groupsUpdated";
        }
    }

    CommittingTransaction &t = d->commitTransaction(QList<Event>() << event);
    t.addSignal("eventDeleted",
                Q_ARG(int, id));

    if (groupSignal)
        t.addSignal(groupSignal,
                    Q_ARG(QList<int>, QList<int>() << event.groupId()));

    return true;
}

bool EventModel::deleteEvent(Event &event)
{
    Q_D(EventModel);
    qDebug() << __FUNCTION__ << ":" << event.id();

    if (!event.isValid()) {
        d->lastError = QSqlError();
        d->lastError.setType(QSqlError::TransactionError);
        d->lastError.setDatabaseText("Invalid event");
        qWarning() << __FUNCTION__ << ":" << d->lastError;
        return false;
    }

    d->tracker()->transaction(d->syncOnCommit);

    if (!d->tracker()->deleteEvent(event, d->bgThread)) {
        d->lastError = d->tracker()->lastError();
        if (d->lastError.isValid())
            qWarning() << Q_FUNC_INFO << ":" << d->lastError;
        d->tracker()->rollback();
        return false;
    }

        const char *groupSignal = 0;
    if (event.groupId() != -1 && !event.isDraft()) {
        int total;
        if (!d->tracker()->totalEventsInGroup(event.groupId(), total)) {
            d->lastError = d->tracker()->lastError();
            if (d->lastError.isValid())
                qWarning() << Q_FUNC_INFO << d->lastError;
            d->tracker()->rollback();
            return false;
        }

        if (total == 1) {
            qDebug() << __FUNCTION__ << ": deleting empty group";
            if (!d->tracker()->deleteGroup(event.groupId(), false)) {
                d->lastError = d->tracker()->lastError();
                if (d->lastError.isValid())
                    qWarning() << Q_FUNC_INFO << d->lastError;
                d->tracker()->rollback();
                return false;
            } else {
                groupSignal = "groupsDeleted";
            }
        } else {
            groupSignal = "groupsUpdated";
        }
    }

    CommittingTransaction &t = d->commitTransaction(QList<Event>() << event);
    t.addSignal("eventDeleted",
                Q_ARG(int, event.id()));

    if (groupSignal)
        t.addSignal(groupSignal,
                    Q_ARG(QList<int>, QList<int>() << event.groupId()));

    return true;
}

bool EventModel::moveEvent(Event &event, int groupId)
{
    Q_D(EventModel);
    qDebug() << __FUNCTION__ << ":" << event.id();

    if (!event.isValid()) {
        d->lastError = QSqlError();
        d->lastError.setType(QSqlError::TransactionError);
        d->lastError.setDatabaseText("Invalid event");
        qWarning() << __FUNCTION__ << ":" << d->lastError;
        return false;
    }

    if(event.groupId() == groupId)
    {
        qDebug() << "Event already in proper group";
        return true;
    }

    d->tracker()->transaction(d->syncOnCommit);
    if (!d->tracker()->moveEvent(event, groupId)) {
        d->lastError = d->tracker()->lastError();
        if (d->lastError.isValid())
            qWarning() << Q_FUNC_INFO << ":" << d->lastError;
        d->tracker()->rollback();
        return false;
    }

    emit d->eventDeleted(event.id());
    // update or delete old group
    if (event.groupId() != -1 && !event.isDraft()) {
        int total;
        if (!d->tracker()->totalEventsInGroup(event.groupId(), total)) {
            d->lastError = d->tracker()->lastError();
            if (d->lastError.isValid())
                qWarning() << Q_FUNC_INFO << d->lastError;
            d->tracker()->rollback();
            return false;
        }

        if (total == 1) {
            qDebug() << __FUNCTION__ << ": deleting empty group";
            if (!d->tracker()->deleteGroup(event.groupId(), false)) {
                d->lastError = d->tracker()->lastError();
                if (d->lastError.isValid())
                    qWarning() << Q_FUNC_INFO << "error deleting empty group" ;
                d->tracker()->rollback();
                return false;
            } else {
                emit d->groupsDeleted(QList<int>() << event.groupId());
            }
        } else {
            emit d->groupsUpdated(QList<int>() << event.groupId());
        }
    }

    event.setGroupId(groupId);

    emit d->eventsAdded(QList<Event>() << event);

    d->commitTransaction(QList<Event>() << event);

    return true;
}

bool EventModel::modifyEventsInGroup(QList<Event> &events, Group group)
{
    Q_D(EventModel);

    if (events.isEmpty())
        return true;

    qDebug() << Q_FUNC_INFO;

    d->tracker()->transaction(d->syncOnCommit);
    QMutableListIterator<Event> i(events);
    while (i.hasNext()) {
        Event event = i.next();
        if (event.id() == -1) {
            d->lastError = QSqlError();
            d->lastError.setType(QSqlError::TransactionError);
            d->lastError.setDatabaseText(QLatin1String("Event id not set"));
            qWarning() << Q_FUNC_INFO << ":" << d->lastError;
            d->tracker()->rollback();
            return false;
        }

        if (event.lastModified() == QDateTime::fromTime_t(0)) {
             event.setLastModified(QDateTime::currentDateTime());
        }

        if (!d->tracker()->modifyEvent(event)) {
            d->lastError = d->tracker()->lastError();
            if (d->lastError.isValid())
                qWarning() << Q_FUNC_INFO << ":" << d->lastError;
            d->tracker()->rollback();
            return false;
        }
        if (group.lastEventId() == event.id()) {
            if (event.modifiedProperties().contains(Event::Status))
                group.setLastEventStatus(event.status());
            group.setLastEventType(event.type());
            //text might be changed in case of MMS
            if (event.modifiedProperties().contains(Event::FreeText)
                || event.modifiedProperties().contains(Event::Subject)) {
                if(event.type() == Event::MMSEvent) {
                    group.setLastMessageText(event.subject().isEmpty() ? event.freeText() : event.subject());
                } else {
                    group.setLastMessageText(event.freeText());
                }
            }
            if (event.modifiedProperties().contains(Event::IsRead)
                && event.isRead()) {
                group.setUnreadMessages(qMax(group.unreadMessages() - 1, 0));
            }
            if (event.modifiedProperties().contains(Event::FromVCardFileName)
                || event.modifiedProperties().contains(Event::FromVCardLabel)) {
                group.setLastVCardFileName(event.fromVCardFileName());
                group.setLastVCardLabel(event.fromVCardLabel());
            }
        }
    }

    CommittingTransaction &t = d->commitTransaction(events);
    t.addSignal("eventsUpdated",
                Q_ARG(QList<CommHistory::Event>, events));
    t.addSignal("groupsUpdatedFull",
                Q_ARG(QList<CommHistory::Group>, QList<Group>() << group));

    return true;
}

bool EventModel::canFetchMore(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    Q_D(const EventModel);

    if (!d->queryRunner)
        return false;

    return d->canFetchMore();
}

void EventModel::fetchMore(const QModelIndex &parent)
{
    Q_UNUSED(parent);
    Q_D(EventModel);

    if (d->queryRunner)
        d->queryRunner->fetchMore();
}

void EventModel::setBackgroundThread(QThread *thread)
{
    Q_D(EventModel);

    d->bgThread = thread;
    qDebug() << Q_FUNC_INFO << thread;

    d->resetQueryRunners();
}

QThread* EventModel::backgroundThread()
{
    Q_D(EventModel);

    return d->bgThread;
}

TrackerIO& EventModel::trackerIO()
{
    Q_D(EventModel);

    return *d->tracker();
}
