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

#include "contactgroup.h"
#include "updatesemitter.h"
#include "commonutils.h"
#include "databaseio.h"
#include "debug.h"

namespace CommHistory {

class ContactGroupPrivate {
    Q_DECLARE_PUBLIC(ContactGroup)

public:
    ContactGroupPrivate(ContactGroup *parent);

    void update();

    ContactGroup *q_ptr;
    QList<GroupObject*> groups;
 
    QList<int> contactIds;
    QStringList displayNames;
    quint32 startTimeT, endTimeT, lastModifiedT;
    int unreadMessages;
    int lastEventId;
    GroupObject *lastEventGroup;
    QString lastMessageText, lastVCardFileName, lastVCardLabel;
    int lastEventType, lastEventStatus;
    bool lastEventIsDraft;
};

}

using namespace CommHistory;

ContactGroupPrivate::ContactGroupPrivate(ContactGroup *parent)
    : q_ptr(parent)
    , startTimeT(0)
    , endTimeT(0)
    , lastModifiedT(0)
    , unreadMessages(0)
    , lastEventId(-1)
    , lastEventGroup(0)
    , lastEventType(Event::UnknownType)
    , lastEventStatus(Event::UnknownStatus)
    , lastEventIsDraft(false)
{
}

ContactGroup::ContactGroup(QObject *parent)
    : QObject(parent), d_ptr(new ContactGroupPrivate(this))
{
}

void ContactGroup::addGroup(GroupObject *group)
{
    Q_D(ContactGroup);
    if (!d->groups.contains(group)) {
        d->groups.append(group);
        emit groupsChanged();
    }

    d->update();
}

bool ContactGroup::removeGroup(GroupObject *group)
{
    Q_D(ContactGroup);
    if (d->groups.removeOne(group)) {
        emit groupsChanged();
        d->update();
    }

    return d->groups.isEmpty();
}

void ContactGroup::updateGroup(GroupObject *group)
{
    Q_D(ContactGroup);

    Q_UNUSED(group);
    d->update();
}

void ContactGroupPrivate::update()
{
    Q_Q(ContactGroup);
    /* Iterate all groups to update properties. Unfortunately, we can't rely on just comparing
       the changes to one group and have reliable data.
     */

    QList<int> uContactIds;
    QStringList uDisplayNames;
    quint32 uStartTimeT = 0, uEndTimeT = 0, uLastModifiedT = 0;
    int uUnreadMessages = 0;
    GroupObject *uLastEventGroup = 0;

    if (!groups.isEmpty()) {
        /* Because of the mechanics of hasSameContacts, these values must be the
         * same for all groups, so we can just use the first. */
        uContactIds = groups[0]->recipients().contactIds();
        uDisplayNames = groups[0]->recipients().displayNames();
    }

    foreach (GroupObject *group, groups) {
        const quint32 gStartTimeT = group->startTimeT();
        const quint32 gEndTimeT = group->endTimeT();
        const quint32 gLastModifiedT = group->lastModifiedT();

        uStartTimeT = qMax(gStartTimeT, uStartTimeT);
        uEndTimeT = qMax(gEndTimeT, uEndTimeT);
        uLastModifiedT = qMax(gLastModifiedT, uLastModifiedT);

        if (group->lastEventId() >= 0 && (!uLastEventGroup || gEndTimeT > uLastEventGroup->endTimeT()))
            uLastEventGroup = group;

        uUnreadMessages += group->unreadMessages();
    }

    if (uContactIds != contactIds) {
        contactIds = uContactIds;
        emit q->contactIdsChanged();
    }

    if (uDisplayNames != displayNames) {
        displayNames = uDisplayNames;
        emit q->displayNamesChanged();
    }

    if (uStartTimeT != startTimeT) {
        startTimeT = uStartTimeT;
        emit q->startTimeChanged();
    }

    if (uEndTimeT != endTimeT) {
        endTimeT = uEndTimeT;
        emit q->endTimeChanged();
    }

    if (uLastModifiedT != lastModifiedT) {
        lastModifiedT = uLastModifiedT;
        emit q->lastModifiedChanged();
    }

    if (uUnreadMessages != unreadMessages) {
        unreadMessages = uUnreadMessages;
        emit q->unreadMessagesChanged();
    }

    if (uLastEventGroup) {
        bool changed = false;
        if (uLastEventGroup != lastEventGroup) {
            lastEventGroup = uLastEventGroup;
            changed = true;
        }

        if (lastEventId != lastEventGroup->lastEventId() ||
                lastMessageText != lastEventGroup->lastMessageText() ||
                lastVCardFileName != lastEventGroup->lastVCardFileName() ||
                lastVCardLabel != lastEventGroup->lastVCardLabel() ||
                lastEventType != lastEventGroup->lastEventType() ||
                lastEventStatus != lastEventGroup->lastEventStatus() ||
                lastEventIsDraft != lastEventGroup->lastEventIsDraft())
            changed = true;

        lastEventId = lastEventGroup->lastEventId();
        lastMessageText = lastEventGroup->lastMessageText();
        lastVCardFileName = lastEventGroup->lastVCardFileName();
        lastVCardLabel = lastEventGroup->lastVCardLabel();
        lastEventType = static_cast<int>(lastEventGroup->lastEventType());
        lastEventStatus = static_cast<int>(lastEventGroup->lastEventStatus());
        lastEventIsDraft = lastEventGroup->lastEventIsDraft();

        if (changed)
            emit q->lastEventChanged();
    } else if (lastEventId >= 0) {
        lastEventId = -1;
        lastMessageText.clear();
        lastVCardFileName.clear();
        lastVCardLabel.clear();
        lastEventType = static_cast<int>(Event::UnknownType);
        lastEventStatus = static_cast<int>(Event::UnknownStatus);
        lastEventIsDraft = false;
        emit q->lastEventChanged();
    }
}

QList<int> ContactGroup::contactIds() const
{
    Q_D(const ContactGroup);
    if (d->groups.isEmpty())
        return QList<int>();
    return d->groups[0]->recipients().contactIds();
}

QStringList ContactGroup::displayNames() const
{
    Q_D(const ContactGroup);
    if (d->groups.isEmpty())
        return QStringList();
    return d->groups[0]->recipients().displayNames();
}

QDateTime ContactGroup::startTime() const
{
    Q_D(const ContactGroup);
    return QDateTime::fromTime_t(d->startTimeT);
}

QDateTime ContactGroup::endTime() const
{
    Q_D(const ContactGroup);
    return QDateTime::fromTime_t(d->endTimeT);
}

int ContactGroup::unreadMessages() const
{
    Q_D(const ContactGroup);
    return d->unreadMessages;
}

int ContactGroup::lastEventId() const
{
    Q_D(const ContactGroup);
    return d->lastEventId;
}

GroupObject *ContactGroup::lastEventGroup() const
{
    Q_D(const ContactGroup);
    return d->lastEventGroup;
}

QString ContactGroup::lastMessageText() const
{
    Q_D(const ContactGroup);
    return d->lastMessageText;
}

QString ContactGroup::lastVCardFileName() const
{
    Q_D(const ContactGroup);
    return d->lastVCardFileName;
}

QString ContactGroup::lastVCardLabel() const
{
    Q_D(const ContactGroup);
    return d->lastVCardLabel;
}

int ContactGroup::lastEventType() const
{
    Q_D(const ContactGroup);
    return d->lastEventType;
}

int ContactGroup::lastEventStatus() const
{
    Q_D(const ContactGroup);
    return d->lastEventStatus;
}

bool ContactGroup::lastEventIsDraft() const
{
    Q_D(const ContactGroup);
    return d->lastEventIsDraft;
}

QDateTime ContactGroup::lastModified() const
{
    Q_D(const ContactGroup);
    return QDateTime::fromTime_t(d->lastModifiedT);
}

QList<GroupObject*> ContactGroup::groups() const
{
    Q_D(const ContactGroup);
    return d->groups;
}

QList<QObject*> ContactGroup::groupObjects() const
{
    Q_D(const ContactGroup);
    QList<QObject*> l;
    l.reserve(d->groups.size());
    foreach (GroupObject *o, d->groups)
        l.append(o);
    return l;
}

bool ContactGroup::markAsRead() 
{
    Q_D(ContactGroup);

    DEBUG() << Q_FUNC_INFO;
    if (d->groups.isEmpty() || !unreadMessages())
        return true;

    DatabaseIO *database = DatabaseIO::instance();
    if (!database->transaction())
        return false;

    foreach (GroupObject *group, d->groups) {
        if (group->unreadMessages() && !database->markAsReadGroup(group->id())) {
            database->rollback();
            return false;
        }
    }

    if (!database->commit())
        return false;

    QList<Group> updated;
    foreach (GroupObject *group, d->groups) {
        if (group->unreadMessages()) {
            group->setUnreadMessages(0);
            updated.append(group->toGroup());
        }
    }

    emit UpdatesEmitter::instance()->groupsUpdatedFull(updated);
    return true;
}

bool ContactGroup::deleteGroups()
{
    Q_D(ContactGroup);

    QList<int> ids;
    ids.reserve(d->groups.size());
    foreach (GroupObject *group, d->groups)
        ids.append(group->id());

    if (ids.isEmpty())
        return true;

    DatabaseIO *database = DatabaseIO::instance();
    if (!database->transaction())
        return false;

    if (!database->deleteGroups(ids)) {
        database->rollback();
        return false;
    }

    if (!database->commit())
        return false;

    emit UpdatesEmitter::instance()->groupsDeleted(ids);
    return true;
}

quint32 ContactGroup::startTimeT() const
{
    Q_D(const ContactGroup);
    return d->startTimeT;
}

quint32 ContactGroup::endTimeT() const
{
    Q_D(const ContactGroup);
    return d->endTimeT;
}

quint32 ContactGroup::lastModifiedT() const
{
    Q_D(const ContactGroup);
    return d->lastModifiedT;
}

GroupObject *ContactGroup::findGroup(const QString &localUid, const QString &remoteUid)
{
    return findGroup(localUid, QStringList() << remoteUid);
}

GroupObject *ContactGroup::findGroup(const QString &localUid, const QStringList &remoteUids)
{
    Q_D(ContactGroup);

    RecipientList match = RecipientList::fromUids(localUid, remoteUids);
    foreach (GroupObject *g, d->groups) {
        if (g->localUid() == localUid && g->recipients() == match)
            return g;
    }

    return 0;
}

