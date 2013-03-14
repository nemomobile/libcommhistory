/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: John Brooks <john.brooks@jollamobile.com>
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
#include <QStringList>
#include <QObjectList>

#include "contactgroupmodel.h"
#include "contactgroupmodel_p.h"
#include "groupmanager.h"
#include "contactgroup.h"

Q_DECLARE_METATYPE(QObjectList)

using namespace CommHistory;

inline static bool contactGroupSort(ContactGroup *a, ContactGroup *b)
{
    return a->endTime() > b->endTime(); // descending order
}

ContactGroupModelPrivate::ContactGroupModelPrivate(ContactGroupModel *model)
        : QObject(model)
        , q_ptr(model)
        , manager(0)
{
}

ContactGroupModelPrivate::~ContactGroupModelPrivate()
{
    qDeleteAll(items);
    items.clear();
}

void ContactGroupModelPrivate::setManager(GroupManager *m)
{
    Q_Q(ContactGroupModel);

    if (manager == m)
        return;

    q->beginResetModel();

    if (manager) {
        disconnect(manager, 0, this, 0);
        disconnect(manager, 0, q, 0);

        foreach (ContactGroup *g, items)
            emit q->contactGroupRemoved(g);
        qDeleteAll(items);
        items.clear();
    }

    manager = m;

    if (manager) {
        connect(manager, SIGNAL(groupAdded(GroupObject*)), SLOT(groupAdded(GroupObject*)));
        connect(manager, SIGNAL(groupUpdated(GroupObject*)), SLOT(groupUpdated(GroupObject*)));
        connect(manager, SIGNAL(groupDeleted(GroupObject*)), SLOT(groupDeleted(GroupObject*)));
        connect(manager, SIGNAL(modelReady(bool)), q, SIGNAL(modelReady(bool)));

        // Create data without sorting
        foreach (GroupObject *group, manager->groups()) {
            int index = indexForContacts(group->contactIds());

            if (index < 0) {
                ContactGroup *item = new ContactGroup(this);
                index = items.size();
                items.append(item);
            }

            items[index]->addGroup(group);
            emit q->contactGroupCreated(items[index]);
        }

        qSort(items.begin(), items.end(), contactGroupSort);
    }

    q->endResetModel();

    if (manager->isReady())
        emit q->modelReady(true);
}

int ContactGroupModelPrivate::indexForContacts(QList<int> contacts)
{
    if (contacts.isEmpty())
        return -1;

    std::sort(contacts.begin(), contacts.end());

    for (int i = 0; i < items.size(); i++) {
        QList<int> ic = items[i]->contactIds();
        std::sort(ic.begin(), ic.end());

        if (contacts == ic)
            return i;
    }
    
    return -1;
}

int ContactGroupModelPrivate::indexForObject(GroupObject *group)
{
    for (int i = 0; i < items.size(); i++) {
        if (items[i]->groups().contains(group))
            return i;
    }

    return -1;
}

void ContactGroupModelPrivate::groupAdded(GroupObject *group)
{
    Q_Q(ContactGroupModel);

    qDebug() << Q_FUNC_INFO << group->id() << group->contactIds();

    int index = indexForContacts(group->contactIds());

    if (index < 0) {
        ContactGroup *item = new ContactGroup(this);
        item->addGroup(group);

        for (index = 0; index < items.size(); index++) {
            if (contactGroupSort(item, items[index]))
                break;
        }

        q->beginInsertRows(QModelIndex(), index, index);
        items.insert(index, item);
        q->endInsertRows();

        emit q->contactGroupCreated(item);
        return;
    }

    ContactGroup *item = items[index];
    item->addGroup(group);

    itemDataChanged(index);
}

void ContactGroupModelPrivate::itemDataChanged(int index)
{
    Q_Q(ContactGroupModel);

    int newIndex = index;
    for (int i = index - 1; i >= 0; i--) {
        if (contactGroupSort(items[index], items[i]))
            newIndex = i;
        else
            break;
    }

    for (int i = index + 1; i < items.size(); i++) {
        if (contactGroupSort(items[i], items[index]))
            newIndex = i;
        else
            break;
    }

    qDebug() << Q_FUNC_INFO << index << newIndex;

    if (newIndex != index) {
        q->beginMoveRows(QModelIndex(), index, index, QModelIndex(), newIndex > index ? newIndex + 1 : newIndex);
        qDebug() << Q_FUNC_INFO << "move" << index << newIndex;
        items.move(index, newIndex);
        q->endMoveRows();
    }

    qDebug() << Q_FUNC_INFO << "changed" << newIndex;
    emit q->dataChanged(q->index(newIndex, 0, QModelIndex()),
                        q->index(newIndex, ContactGroupModel::NumberOfColumns-1, QModelIndex()));

    emit q->contactGroupChanged(items[newIndex]);
}

void ContactGroupModelPrivate::groupUpdated(GroupObject *group)
{
    int oldIndex = indexForObject(group);
    int newIndex;
    
    // If the group has any contact information, and there is more than one group in this contactgroup,
    // check for a new contactgroup. Otherwise, the current one is used.
    if (!group->contactIds().isEmpty() && (oldIndex >= 0 && items[oldIndex]->groups().size() > 1))
        newIndex = indexForContacts(group->contactIds());
    else
        newIndex = oldIndex;

    qDebug() << Q_FUNC_INFO << group->id() << group->contactIds() << oldIndex << newIndex;

    if (oldIndex >= 0 && oldIndex != newIndex) {
        // Remove from old
        groupDeleted(group);
    }

    if (newIndex < 0 || oldIndex != newIndex) {
        // Add to new, creating if necessary
        groupAdded(group);
    } else {
        // Update data
        items[oldIndex]->updateGroup(group);
        itemDataChanged(oldIndex);
    }
}

void ContactGroupModelPrivate::groupDeleted(GroupObject *group)
{
    Q_Q(ContactGroupModel);

    qDebug() << Q_FUNC_INFO << group->id() << group->contactIds();

    int index = indexForObject(group);
    if (index < 0)
        return;

    ContactGroup *item = items[index];
    // Returns true when removing the last group
    if (item->removeGroup(group)) {
        emit q->beginRemoveRows(QModelIndex(), index, index);
        items.removeAt(index);
        emit q->endRemoveRows();

        emit q->contactGroupRemoved(item);

        delete item;
        return;
    }

    itemDataChanged(index);
}

ContactGroupModel::ContactGroupModel(QObject *parent)
    : QAbstractTableModel(parent),
      d(new ContactGroupModelPrivate(this))
{
    qRegisterMetaType<QList<CommHistory::GroupObject*> >();
    qRegisterMetaType<CommHistory::GroupObject*>();
    qRegisterMetaType<CommHistory::ContactGroup*>();

    QHash<int,QByteArray> roles;
    roles[ContactGroupRole] = "contactGroup";
    roles[BaseRole + ContactIds] = "contactIds";
    roles[BaseRole + ContactNames] = "contactNames";
    roles[BaseRole + EndTime] = "endTime";
    roles[BaseRole + TotalMessages] = "totalMessages";
    roles[BaseRole + UnreadMessages] = "unreadMessages";
    roles[BaseRole + SentMessages] = "sentMessages";
    roles[BaseRole + LastEventGroup] = "lastEventGroup";
    roles[BaseRole + LastEventId] = "lastEventId";
    roles[BaseRole + LastMessageText] = "lastMessageText";
    roles[BaseRole + LastVCardFileName] = "lastVCardFileName";
    roles[BaseRole + LastVCardLabel] = "lastVCardLabel";
    roles[BaseRole + LastEventType] = "lastEventType";
    roles[BaseRole + LastEventStatus] = "lastEventStatus";
    roles[BaseRole + LastModified] = "lastModified";
    roles[BaseRole + StartTime] = "startTime";
    roles[BaseRole + Groups] = "groups";
    setRoleNames(roles);
}

ContactGroupModel::~ContactGroupModel()
{
    delete d;
    d = 0;
}

GroupManager *ContactGroupModel::manager() const
{
    return d->manager;
}

void ContactGroupModel::setManager(GroupManager *m)
{
    if (d->manager == m)
        return;

    d->setManager(m);
    emit managerChanged();
}

int ContactGroupModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->items.count();
}

int ContactGroupModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return NumberOfColumns;
}

QVariant ContactGroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= d->items.count()) {
        return QVariant();
    }

    ContactGroup *g = d->items.at(index.row());

    int column = index.column();
    if (role >= BaseRole) {
        column = role - BaseRole;
        role = Qt::DisplayRole;
    }

    if (role == ContactGroupRole)
        return QVariant::fromValue<QObject*>(g);

    if (role != Qt::DisplayRole)
        return QVariant();

    QVariant var;
    switch (column) {
        case ContactIds:
            var = QVariant::fromValue(g->contactIds());
            break;
        case ContactNames:
            var = QVariant::fromValue<QStringList>(g->contactNames());
            break;
        case Groups:
            var = QVariant::fromValue<QObjectList>(g->groupObjects());
            break;
        case EndTime:
            var = g->endTime();
            break;
        case TotalMessages:
            var = g->totalMessages();
            break;
        case UnreadMessages:
            var = g->unreadMessages();
            break;
        case SentMessages:
            var = g->sentMessages();
            break;
        case LastEventGroup:
            var = QVariant::fromValue<QObject*>(g->lastEventGroup());
            break;
        case LastEventId:
            var = g->lastEventId();
            break;
        case LastMessageText:
            var = g->lastMessageText();
            break;
        case LastVCardFileName:
            var = g->lastVCardFileName();
            break;
        case LastVCardLabel:
            var = g->lastVCardLabel();
            break;
        case LastEventType:
            var = g->lastEventType();
            break;
        case LastEventStatus:
            var = g->lastEventStatus();
            break;
        case LastModified:
            var = g->lastModified();
            break;
        case StartTime:
            var = g->startTime();
            break;
        default:
            qDebug() << "ContactGroupModel::data: invalid column id??" << column;
            break;
    }

    return var;
}

ContactGroup *ContactGroupModel::at(const QModelIndex &index) const
{
    return d->items.value(index.row());
}

QObjectList ContactGroupModel::contactGroups() const
{
    QObjectList re;
    re.reserve(d->items.size());

    foreach (ContactGroup *g, d->items)
        re.append(g);

    return re;
}

bool ContactGroupModel::canFetchMore(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return d->manager ? d->manager->canFetchMore() : false;
    return false;
}

void ContactGroupModel::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid() && d->manager)
        d->manager->fetchMore();
}

