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
    Q_DECLARE_PUBLIC(ContactGroup);

public:
    ContactGroupPrivate(ContactGroup *parent);

    void update();

    ContactGroup *q_ptr;
    QList<GroupObject*> groups;
 
    QList<int> contactIds;
    QList<QString> contactNames;
    QDateTime startTime, endTime, lastModified;
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

    QMap<int,QString> contacts;
 
    QDateTime uStartTime, uEndTime, uLastModified;
    int uUnreadMessages = 0;
    GroupObject *uLastEventGroup = 0;

    foreach (GroupObject *group, groups) {
        foreach (const Event::Contact &contact, group->contacts())
            contacts[contact.first] = contact.second;

        if (!uStartTime.isValid() || group->startTime() > uStartTime)
            uStartTime = group->startTime();

        if (!uEndTime.isValid() || group->endTime() > uEndTime)
            uEndTime = group->endTime();

        if (group->lastEventId() >= 0 && (!uLastEventGroup || group->endTime() > uLastEventGroup->endTime()))
            uLastEventGroup = group;

        if (!uLastModified.isValid() || group->lastModified() > uLastModified)
            uLastModified = group->lastModified();

        uUnreadMessages += group->unreadMessages();
    }

    QList<int> uContactIds = contacts.keys();
    QList<QString> uContactNames = contacts.values();

    if (uContactIds != contactIds || uContactNames != contactNames) {
        contactIds = uContactIds;
        contactNames = uContactNames;
        emit q->contactsChanged();
    }

    if (uStartTime != startTime) {
        startTime = uStartTime;
        emit q->startTimeChanged();
    }

    if (uEndTime != endTime) {
        endTime = uEndTime;
        emit q->endTimeChanged();
    }

    if (uLastModified != lastModified) {
        lastModified = uLastModified;
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
    return d->contactIds;
}

QStringList ContactGroup::contactNames() const
{
    Q_D(const ContactGroup);
    return d->contactNames;
}

QDateTime ContactGroup::startTime() const
{
    Q_D(const ContactGroup);
    return d->startTime;
}

QDateTime ContactGroup::endTime() const
{
    Q_D(const ContactGroup);
    return d->endTime;
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
    return d->lastModified;
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

GroupObject *ContactGroup::findGroup(const QString &localUid, const QString &remoteUid)
{
    return findGroup(localUid, QStringList() << remoteUid);
}

GroupObject *ContactGroup::findGroup(const QString &localUid, const QStringList &remoteUids)
{
    Q_D(ContactGroup);

    foreach (GroupObject *g, d->groups) {
        if (g->localUid() == localUid && g->remoteUids().size() == remoteUids.size()
                && CommHistory::remoteAddressMatch(localUid, g->remoteUids(), remoteUids))
            return g;
    }

    return 0;
}

