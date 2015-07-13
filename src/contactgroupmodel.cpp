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

#include "contactgroupmodel.h"
#include "groupmanager.h"
#include "contactgroup.h"
#include "commonutils.h"
#include "debug.h"

using namespace CommHistory;

namespace {

bool initializeTypes()
{
    qRegisterMetaType<QList<CommHistory::GroupObject*> >();
    qRegisterMetaType<CommHistory::GroupObject*>();
    qRegisterMetaType<CommHistory::ContactGroup*>();
    return true;
}

inline bool contactGroupSort(ContactGroup *a, ContactGroup *b)
{
    return a->endTime() > b->endTime(); // descending order
}

}

bool contactgroupmodel_initialized = initializeTypes();

namespace CommHistory {

class GroupManager;
class GroupObject;
class ContactGroup;

class ContactGroupModelPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactGroupModel)

public:
    ContactGroupModel *q_ptr;

    ContactGroupModelPrivate(ContactGroupModel *parent);
    virtual ~ContactGroupModelPrivate();

    GroupManager *manager;
    QList<ContactGroup*> items;

    void setManager(GroupManager *manager);

    int indexForContacts(GroupObject *group);
    int indexForObject(GroupObject *group);

private slots:
    void groupAdded(GroupObject *group);
    void groupUpdated(GroupObject *group);
    void groupDeleted(GroupObject *group);

private:
    void itemDataChanged(int index);
};

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
            int index = indexForContacts(group);

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

int ContactGroupModelPrivate::indexForContacts(GroupObject *group)
{
    const RecipientList &searchRecipients = group->recipients();

    for (int i = 0; i < items.size(); i++) {
        int matched = 0;
        /* We have to match all groups to be sure that a contact change hasn't
         * invalidated the relationship */
        foreach (GroupObject *compareGroup, items[i]->groups()) {
            const RecipientList &compareRecipients = compareGroup->recipients();

            /* Multi-recipient groups are never combined, because that would create a
             * huge set of nasty corner cases, e.g. when two groups match in contacts
             * but not UIDs. */
            if (searchRecipients.size() > 1 || compareRecipients.size() > 1) {
                if (!searchRecipients.matches(compareRecipients))
                    break;
            } else if (!searchRecipients.hasSameContacts(compareRecipients)) {
                break;
            }

            matched++;
        }

        if (matched > 0 && matched == items[i]->groups().size())
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

    int index = indexForContacts(group);

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

    if (newIndex != index) {
        q->beginMoveRows(QModelIndex(), index, index, QModelIndex(), newIndex > index ? newIndex + 1 : newIndex);
        items.move(index, newIndex);
        q->endMoveRows();
    }

    emit q->dataChanged(q->index(newIndex, 0, QModelIndex()),
                        q->index(newIndex, ContactGroupModel::NumberOfColumns-1, QModelIndex()));

    emit q->contactGroupChanged(items[newIndex]);
}

void ContactGroupModelPrivate::groupUpdated(GroupObject *group)
{
    int oldIndex = indexForObject(group);
    int newIndex = -1;
    
    if (oldIndex >= 0) {
        newIndex = indexForContacts(group);

        if (oldIndex != newIndex) {
            // Remove from old
            groupDeleted(group);
        }
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
}

QHash<int,QByteArray> ContactGroupModel::roleNames() const
{
    QHash<int,QByteArray> roles;
    roles[ContactGroupRole] = "contactGroup";
    roles[WeekdaySectionRole] = "weekdaySection";
    roles[BaseRole + ContactIds] = "contactIds";
    roles[BaseRole + ContactNames] = "contactNames"; // TODO: Obsolete, remove
    roles[BaseRole + EndTime] = "endTime";
    roles[BaseRole + UnreadMessages] = "unreadMessages";
    roles[BaseRole + LastEventGroup] = "lastEventGroup";
    roles[BaseRole + LastEventId] = "lastEventId";
    roles[BaseRole + LastMessageText] = "lastMessageText";
    roles[BaseRole + LastVCardFileName] = "lastVCardFileName";
    roles[BaseRole + LastVCardLabel] = "lastVCardLabel";
    roles[BaseRole + LastEventType] = "lastEventType";
    roles[BaseRole + LastEventStatus] = "lastEventStatus";
    roles[BaseRole + LastEventIsDraft] = "lastEventIsDraft";
    roles[BaseRole + LastModified] = "lastModified";
    roles[BaseRole + StartTime] = "startTime";
    roles[BaseRole + Groups] = "groups";
    roles[BaseRole + DisplayNames] = "displayNames";
    return roles;
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

    if (role == ContactGroupRole) {
        return QVariant::fromValue<QObject*>(g);
    } else if (role == WeekdaySectionRole) {
        QDateTime dateTime = g->endTime().toLocalTime();

        // Return the date for the past week, and group all older items together under an
        // arbitrary older date
        const int daysDiff = QDate::currentDate().toJulianDay() - dateTime.date().toJulianDay();
        if (daysDiff < 7)
            return dateTime.date();

        // Arbitrary static date for older items..
        return QDate(2000, 1, 1);
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    QVariant var;
    switch (column) {
        case ContactIds:
            var = QVariant::fromValue(g->contactIds());
            break;
        case ContactNames:
        case DisplayNames:
            var = QVariant::fromValue(g->displayNames());
            break;
        case Groups:
            var = QVariant::fromValue<QList<QObject*> >(g->groupObjects());
            break;
        case EndTime:
            var = g->endTime();
            break;
        case UnreadMessages:
            var = g->unreadMessages();
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
        case LastEventIsDraft:
            var = g->lastEventIsDraft();
            break;
        case LastModified:
            var = g->lastModified();
            break;
        case StartTime:
            var = g->startTime();
            break;
        default:
            DEBUG() << "ContactGroupModel::data: invalid column id??" << column;
            break;
    }

    return var;
}

ContactGroup *ContactGroupModel::at(const QModelIndex &index) const
{
    return d->items.value(index.row());
}

QList<QObject*> ContactGroupModel::contactGroups() const
{
    QList<QObject*> re;
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

#include "contactgroupmodel.moc"

