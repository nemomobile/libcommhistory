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

#include "commonutils.h"
#include "groupmodel.h"
#include "groupmodel_p.h"
#include "eventmodel.h"
#include "group.h"
#include "groupmanager.h"
#include "groupobject.h"
#include "debug.h"

using namespace CommHistory;

namespace {

bool initializeTypes()
{
    qRegisterMetaType<QList<CommHistory::Event> >();
    qRegisterMetaType<QList<CommHistory::Group> >();
    qRegisterMetaType<QList<int> >();
    return true;
}

inline bool groupObjectSort(GroupObject *a, GroupObject *b)
{
    return a->endTimeT() > b->endTimeT(); // descending order
}

}

bool groupmodel_initialized = initializeTypes();

GroupModelPrivate::GroupModelPrivate(GroupModel *model)
        : q_ptr(model)
        , manager(0)
{
}

GroupModelPrivate::~GroupModelPrivate()
{
}

void GroupModelPrivate::ensureManager()
{
    if (!manager)
        setManager(new GroupManager(this));
}

void GroupModelPrivate::setManager(GroupManager *m)
{
    Q_Q(GroupModel);

    if (m == manager)
        return;

    q->beginResetModel();
    groups.clear();

    if (manager) {
        disconnect(manager, 0, this, 0);
        disconnect(manager, 0, q, 0);
    }

    manager = m;

    if (manager) {
        connect(manager, SIGNAL(groupAdded(GroupObject*)), SLOT(groupAdded(GroupObject*)));
        connect(manager, SIGNAL(groupUpdated(GroupObject*)), SLOT(groupUpdated(GroupObject*)));
        connect(manager, SIGNAL(groupDeleted(GroupObject*)), SLOT(groupDeleted(GroupObject*)));

        connect(manager, SIGNAL(modelReady(bool)), q, SIGNAL(modelReady(bool)));
        connect(manager, SIGNAL(groupsCommitted(QList<int>,bool)), q, SIGNAL(groupsCommitted(QList<int>,bool)));

        groups = manager->groups();
        qSort(groups.begin(), groups.end(), groupObjectSort);
    }

    q->endResetModel();
    if (manager && manager->isReady())
        emit q->modelReady(true);
}

void GroupModelPrivate::groupAdded(GroupObject *group)
{
    Q_Q(GroupModel);

    int index = 0;
    for (; index < groups.size(); index++) {
        if (groupObjectSort(group, groups[index]))
            break;
    }

    q->beginInsertRows(QModelIndex(), index, index);
    groups.insert(index, group);
    q->endInsertRows();
}

void GroupModelPrivate::groupUpdated(GroupObject *group)
{
    Q_Q(GroupModel);

    int index = groups.indexOf(group);
    if (index < 0)
        return;

    int newIndex = index;
    for (int i = index - 1; i >= 0; i--) {
        if (groupObjectSort(group, groups[i]))
            newIndex = i;
        else
            break;
    }

    for (int i = index + 1; i < groups.size(); i++) {
        if (groupObjectSort(groups[i], group))
            newIndex = i;
        else
            break;
    }

    DEBUG() << Q_FUNC_INFO << index << newIndex;

    if (newIndex != index) {
        q->beginMoveRows(QModelIndex(), index, index, QModelIndex(), newIndex > index ? newIndex + 1 : newIndex);
        DEBUG() << Q_FUNC_INFO << "move" << index << newIndex;
        groups.move(index, newIndex);
        q->endMoveRows();
    }

    emit q->dataChanged(q->index(newIndex, 0, QModelIndex()),
                        q->index(newIndex, GroupModel::NumberOfColumns-1, QModelIndex()));
}

void GroupModelPrivate::groupDeleted(GroupObject *group)
{
    Q_Q(GroupModel);

    int index = groups.indexOf(group);
    if (index < 0)
        return;

    q->beginRemoveRows(QModelIndex(), index, index);
    groups.removeAt(index);
    q->endRemoveRows();
}

GroupModel::GroupModel(QObject *parent)
    : QAbstractTableModel(parent),
      d(new GroupModelPrivate(this))
{
}

QHash<int, QByteArray> GroupModel::roleNames() const
{
    QHash<int,QByteArray> roles;
    roles[BaseRole + GroupId] = "groupId";
    roles[BaseRole + LocalUid] = "localUid";
    roles[BaseRole + RemoteUids] = "remoteUids";
    roles[BaseRole + ChatName] = "chatName";
    roles[BaseRole + EndTime] = "endTime";
    roles[BaseRole + UnreadMessages] = "unreadMessages";
    roles[BaseRole + LastEventId] = "lastEventId";
    roles[BaseRole + Contacts] = "contacts";
    roles[BaseRole + LastMessageText] = "lastMessageText";
    roles[BaseRole + LastVCardFileName] = "lastVCardFileName";
    roles[BaseRole + LastVCardLabel] = "lastVCardLabel";
    roles[BaseRole + LastEventType] = "lastEventType";
    roles[BaseRole + LastEventStatus] = "lastEventStatus";
    roles[BaseRole + IsPermanent] = "isPermanent";
    roles[BaseRole + LastModified] = "lastModified";
    roles[BaseRole + StartTime] = "startTime";
    roles[ContactIdsRole] = "contactIds";
    roles[GroupObjectRole] = "group";
    roles[WeekdaySectionRole] = "weekdaySection";
    return roles;
}

GroupModel::~GroupModel()
{
    delete d;
    d = 0;
}

GroupManager *GroupModel::manager() const
{
    return d->manager;
}

void GroupModel::setManager(GroupManager *m)
{
    if (m == d->manager)
        return;

    d->setManager(m);
}

void GroupModel::setQueryMode(EventModel::QueryMode mode)
{
    d->ensureManager();
    d->manager->setQueryMode(mode);
}

void GroupModel::setChunkSize(uint size)
{
    d->ensureManager();
    d->manager->setChunkSize(size);
}

void GroupModel::setFirstChunkSize(uint size)
{
    d->ensureManager();
    d->manager->setFirstChunkSize(size);
}

void GroupModel::setLimit(int limit)
{
    d->ensureManager();
    d->manager->setLimit(limit);
}

void GroupModel::setOffset(int offset)
{
    d->ensureManager();
    d->manager->setOffset(offset);
}

int GroupModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->groups.count();
}

int GroupModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return NumberOfColumns;
}

QVariant GroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= d->groups.count()) {
        return QVariant();
    }

    GroupObject *group = d->groups.value(index.row());
    if (!group)
        return QVariant();

    if (role == GroupRole) {
        return QVariant::fromValue(group->toGroup());
    } else if (role == GroupObjectRole) {
        return QVariant::fromValue<QObject*>(group);
    } else if (role == ContactIdsRole) {
        return QVariant::fromValue<QList<int> >(group->recipients().contactIds());
    } else if (role == WeekdaySectionRole) {
        QDateTime dateTime = group->endTime().toLocalTime();

        // Return the date for the past week, and group all older items together under an
        // arbitrary older date
        const int daysDiff = QDate::currentDate().toJulianDay() - dateTime.date().toJulianDay();
        if (daysDiff < 7)
            return dateTime.date();

        // Arbitrary static date for older items..
        return QDate(2000, 1, 1);
    }

    int column = index.column();
    if (role >= BaseRole) {
        column = role - BaseRole;
        role = Qt::DisplayRole;
    }
    
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    QVariant var;
    switch (column) {
        case GroupId:
            var = QVariant::fromValue(group->id());
            break;
        case LocalUid:
            var = QVariant::fromValue(group->localUid());
            break;
        case RemoteUids:
            var = QVariant::fromValue(group->recipients().remoteUids());
            break;
        case ChatName:
            var = QVariant::fromValue(group->chatName());
            break;
        case EndTime:
            var = QVariant::fromValue(group->endTime());
            break;
        case UnreadMessages:
            var = QVariant::fromValue(group->unreadMessages());
            break;
        case LastEventId:
            var = QVariant::fromValue(group->lastEventId());
            break;
        case Contacts:
            var = QVariant::fromValue(group->recipients().contactIds());
            break;
        case LastMessageText:
            var = QVariant::fromValue(group->lastMessageText());
            break;
        case LastVCardFileName:
            var = QVariant::fromValue(group->lastVCardFileName());
            break;
        case LastVCardLabel:
            var = QVariant::fromValue(group->lastVCardLabel());
            break;
        case LastEventType:
            var = QVariant::fromValue((int)group->lastEventType());
            break;
        case LastEventStatus:
            var = QVariant::fromValue((int)group->lastEventStatus());
            break;
        case LastModified:
            var = QVariant::fromValue(group->lastModified());
            break;
        case StartTime:
            var = QVariant::fromValue(group->startTime());
            break;
        default:
            DEBUG() << "Group::data: invalid column id??" << column;
            break;
    }

    return var;
}

Group GroupModel::group(const QModelIndex &index) const
{
    GroupObject *go = d->groups.value(index.row());
    if (go)
        return go->toGroup();
    return Group();
}

GroupObject *GroupModel::groupObject(const QModelIndex &index) const
{
    return d->groups.value(index.row());
}

QVariant GroupModel::headerData(int section,
                                Qt::Orientation orientation,
                                int role) const
{
    QVariant var;

    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        QString name;
        switch (section) {
            case GroupId:
                name = QLatin1String("id");
                break;
            case LocalUid:
                name = QLatin1String("local_uid");
                break;
            case RemoteUids:
                name = QLatin1String("remote_uids");
                break;
            case ChatName:
                name = QLatin1String("chat_name");
                break;
            case EndTime:
                name = QLatin1String("end_time");
                break;
            case UnreadMessages:
                name = QLatin1String("unread_messages");
                break;
            case LastEventId:
                name = QLatin1String("last_event_id");
                break;
            case Contacts:
                name = QLatin1String("contacts");
                break;
            case LastMessageText:
                name = QLatin1String("last_message_text");
                break;
            case LastVCardFileName:
                name = QLatin1String("last_vcard_filename");
                break;
            case LastVCardLabel:
                name = QLatin1String("last_vcard_label");
                break;
            case LastEventType:
                name = QLatin1String("last_event_type");
                break;
            case LastEventStatus:
                name = QLatin1String("last_event_status");
                break;
            case LastModified:
                name = QLatin1String("last_modified");
                break;
            case StartTime:
                name = QLatin1String("start_time");
                break;
            default:
                break;
        }
        var = QVariant::fromValue(name);
    }

    return var;
}

bool GroupModel::addGroup(Group &group)
{
    d->ensureManager();
    return d->manager->addGroup(group);
}

bool GroupModel::addGroups(QList<Group> &groups)
{
    d->ensureManager();
    return d->manager->addGroups(groups);
}

bool GroupModel::modifyGroup(Group &group)
{
    d->ensureManager();
    return d->manager->modifyGroup(group);
}

bool GroupModel::getGroups(const QString &localUid,
                           const QString &remoteUid)
{
    d->ensureManager();
    return d->manager->getGroups(localUid, remoteUid);
}

bool GroupModel::markAsReadGroup(int id)
{
    d->ensureManager();
    return d->manager->markAsReadGroup(id);
}

void GroupModel::updateGroups(QList<Group> &groups)
{
    d->ensureManager();
    return d->manager->updateGroups(groups);
}

bool GroupModel::deleteGroups(const QList<int> &groupIds)
{
    d->ensureManager();
    return d->manager->deleteGroups(groupIds);
}

bool GroupModel::deleteAll()
{
    d->ensureManager();
    return d->manager->deleteAll();
}

bool GroupModel::canFetchMore(const QModelIndex &parent) const
{
    if (parent.isValid() || !d->manager)
        return false;

    return d->manager->canFetchMore();
}

void GroupModel::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid() && d->manager)
        d->manager->fetchMore();
}

bool GroupModel::isReady() const
{
    return d->manager && d->manager->isReady();
}

EventModel::QueryMode GroupModel::queryMode() const
{
    d->ensureManager();
    return d->manager->queryMode();
}

uint GroupModel::chunkSize() const
{
    d->ensureManager();
    return d->manager->chunkSize();
}

uint GroupModel::firstChunkSize() const
{
    d->ensureManager();
    return d->manager->firstChunkSize();
}

int GroupModel::limit() const
{
    d->ensureManager();
    return d->manager->limit();
}

int GroupModel::offset() const
{
    d->ensureManager();
    return d->manager->offset();
}

void GroupModel::setBackgroundThread(QThread *thread)
{
    d->ensureManager();
    d->manager->setBackgroundThread(thread);
}

QThread* GroupModel::backgroundThread()
{
    d->ensureManager();
    return d->manager->backgroundThread();
}

DatabaseIO& GroupModel::databaseIO()
{
    d->ensureManager();
    return d->manager->databaseIO();
}

void GroupModel::enableContactChanges(bool enabled)
{
    d->ensureManager();
    d->manager->enableContactChanges(enabled);
}

