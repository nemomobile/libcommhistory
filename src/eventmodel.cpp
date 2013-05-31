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

using namespace CommHistory;

#define MAX_ADD_EVENTS_SIZE 25

EventModel::EventModel(QObject *parent)
        : QAbstractItemModel(parent)
        , d_ptr(new EventModelPrivate(this))
{
    connect(d_ptr, SIGNAL(modelReady(bool)), this, SIGNAL(modelReady(bool)));
    connect(d_ptr, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,bool)),
            this, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,bool)));

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roleNames());
#endif
}

EventModel::EventModel(EventModelPrivate &dd, QObject *parent)
        : QAbstractItemModel(parent), d_ptr(&dd)
{
    connect(d_ptr, SIGNAL(modelReady(bool)), this, SIGNAL(modelReady(bool)));
    connect(d_ptr, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,bool)),
            this, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,bool)));

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roleNames());
#endif
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
    roles[BaseRole + Contacts] = "contacts";
    roles[BaseRole + FreeText] = "freeText";
    roles[BaseRole + GroupId] = "groupId";
    roles[BaseRole + MessageToken] = "messageToken";
    roles[BaseRole + LastModified] = "lastModified";
    roles[BaseRole + EventCount] = "eventCount";
    roles[BaseRole + FromVCardFileName] = "fromVCardFileName";
    roles[BaseRole + FromVCardLabel] = "fromVCardLabel";
    roles[BaseRole + Encoding] = "encoding";
    roles[BaseRole + Charset] = "charset";
    roles[BaseRole + Language] = "language";
    roles[BaseRole + IsDeleted] = "isDeleted";
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
            qDebug() << __PRETTY_FUNCTION__ << ": invalid column id??" << column;
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

        QList<Event> events;
        events << event;
        CommittingTransaction *t = d->commitTransaction(events);

        // signal localy initiated events right away
        // and delay incoming events till they committed
        if (event.direction() != Event::Outbound && t)
            t->addSignal(false,
                         d,
                         "eventsAdded",
                        Q_ARG(QList<CommHistory::Event>, events));
        else
            emit d->eventsAdded(events);
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

            CommittingTransaction *t = d->commitTransaction(added);
            if (t)
                t->addSignal(false,
                             d,
                             "eventsAdded",
                            Q_ARG(QList<CommHistory::Event>, added));

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
        qWarning() << Q_FUNC_INFO << "Event id not set";
        d->tracker()->rollback();
        return false;
    }

    if (event.lastModified() == QDateTime::fromTime_t(0)) {
         event.setLastModified(QDateTime::currentDateTime());
    }

    if (!d->tracker()->modifyEvent(event)) {
        d->tracker()->rollback();
        return false;
    }

    QList<Event> events;
    events << event;
    CommittingTransaction *t = d->commitTransaction(events);

    if (t) {
        // add model update signals to emit on transaction commit
        t->addSignal(false,
                     d,
                     "eventsUpdated",
                    Q_ARG(QList<CommHistory::Event>, events));
        if (event.isValid()
            && event.groupId() != -1
            && !event.isDraft())
            t->addSignal(false,
                         d,
                         "groupsUpdated",
                        Q_ARG(QList<int>, QList<int>() << event.groupId()));
    }

    qDebug() << __FUNCTION__ << ": updated event" << event.id();

    return t != 0;
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
            qWarning() << Q_FUNC_INFO << "Event id not set";
            d->tracker()->rollback();
            return false;
        }

        if (event.lastModified() == QDateTime::fromTime_t(0)) {
             event.setLastModified(QDateTime::currentDateTime());
        }

        if (!d->tracker()->modifyEvent(event)) {
            d->tracker()->rollback();
            return false;
        }

        if (event.isValid()
            && event.groupId() != -1
            && !event.isDraft()
            && !modifiedGroups.contains(event.groupId())) {
            modifiedGroups.append(event.groupId());
        }
    }

    CommittingTransaction *t = d->commitTransaction(events);

    if (t) {
        t->addSignal(false, d, "eventsUpdated", Q_ARG(QList<CommHistory::Event>, events));
        if (!modifiedGroups.isEmpty())
            t->addSignal(false, d, "groupsUpdated",
                        Q_ARG(QList<int>, modifiedGroups));
    }

    return t != 0;
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
            d->tracker()->rollback();
            return false;
        }
        if (total == 1) {
            qDebug() << __FUNCTION__ << ": deleting empty group";
            if  (!d->tracker()->deleteGroup(event.groupId(), false)) {
                d->tracker()->rollback();
                return false;
            } else {
                groupSignal = "groupsDeleted";
            }
        } else {
            groupSignal = "groupsUpdated";
        }
    }

    CommittingTransaction *t = d->commitTransaction(QList<Event>() << event);
    if (t) {
        t->addSignal(false, d, "eventDeleted",
                    Q_ARG(int, id));

        if (groupSignal)
            t->addSignal(false, d, groupSignal,
                        Q_ARG(QList<int>, QList<int>() << event.groupId()));
    }

    return t != 0;
}

bool EventModel::deleteEvent(Event &event)
{
    Q_D(EventModel);
    qDebug() << __FUNCTION__ << ":" << event.id();

    if (!event.isValid()) {
        qWarning() << __FUNCTION__ << "Invalid event";
        return false;
    }

    d->tracker()->transaction(d->syncOnCommit);

    if (!d->tracker()->deleteEvent(event, d->bgThread)) {
        d->tracker()->rollback();
        return false;
    }

        const char *groupSignal = 0;
    if (event.groupId() != -1 && !event.isDraft()) {
        int total;
        if (!d->tracker()->totalEventsInGroup(event.groupId(), total)) {
            d->tracker()->rollback();
            return false;
        }

        if (total == 1) {
            qDebug() << __FUNCTION__ << ": deleting empty group";
            if (!d->tracker()->deleteGroup(event.groupId(), false)) {
                d->tracker()->rollback();
                return false;
            } else {
                groupSignal = "groupsDeleted";
            }
        } else {
            groupSignal = "groupsUpdated";
        }
    }

    CommittingTransaction *t = d->commitTransaction(QList<Event>() << event);

    if (t) {
        t->addSignal(false, d, "eventDeleted",
                    Q_ARG(int, event.id()));

        if (groupSignal)
            t->addSignal(false, d, groupSignal,
                        Q_ARG(QList<int>, QList<int>() << event.groupId()));
    }

    return t != 0;
}

bool EventModel::moveEvent(Event &event, int groupId)
{
    Q_D(EventModel);
    qDebug() << __FUNCTION__ << ":" << event.id();

    if (!event.isValid()) {
        qWarning() << __FUNCTION__ << "Invalid event";
        return false;
    }

    if(event.groupId() == groupId) {
        qDebug() << "Event already in proper group";
        return true;
    }

    d->tracker()->transaction(d->syncOnCommit);
    if (!d->tracker()->moveEvent(event, groupId)) {
        d->tracker()->rollback();
        return false;
    }

    int groupDeleted = -1;
    // update or delete old group
    if (event.groupId() != -1 && !event.isDraft()) {
        int total;
        if (!d->tracker()->totalEventsInGroup(event.groupId(), total)) {
            d->tracker()->rollback();
            return false;
        }

        if (total == 1) {
            qDebug() << __FUNCTION__ << ": deleting empty group";
            if (!d->tracker()->deleteGroup(event.groupId(), false)) {
                qWarning() << Q_FUNC_INFO << "error deleting empty group" ;
                d->tracker()->rollback();
                return false;
            } else {
                groupDeleted = event.groupId();
            }
        }
    }

    event.setGroupId(groupId);

    CommittingTransaction *t = d->commitTransaction(QList<Event>() << event);
    t->addSignal(false, d, "eventDeleted", Q_ARG(int, event.id()));

    if (groupDeleted != -1)
        t->addSignal(false, d, "groupsDeleted",
                     Q_ARG(QList<int>, QList<int>() << groupDeleted));
    else
        t->addSignal(false, d, "groupsUpdated",
                     Q_ARG(QList<int>, QList<int>() << event.groupId()));

    t->addSignal(false, d, "eventsAdded",
                 Q_ARG(QList<CommHistory::Event>, QList<CommHistory::Event>() << event));

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
            qWarning() << Q_FUNC_INFO << "Event id not set";
            d->tracker()->rollback();
            return false;
        }

        if (event.lastModified() == QDateTime::fromTime_t(0)) {
             event.setLastModified(QDateTime::currentDateTime());
        }

        if (!d->tracker()->modifyEvent(event)) {
            d->tracker()->rollback();
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

    CommittingTransaction *t = d->commitTransaction(events);

    if (t) {
        t->addSignal(false,
                     d,
                     "eventsUpdated",
                    Q_ARG(QList<CommHistory::Event>, events));
        t->addSignal(false,
                     d,
                     "groupsUpdatedFull",
                    Q_ARG(QList<CommHistory::Group>, QList<Group>() << group));

    }

    return t != 0;
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
