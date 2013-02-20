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
#include "trackerio.h"
#include "updatesemitter.h"

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
    int totalMessages, unreadMessages, sentMessages;
    int lastEventId;
    GroupObject *lastEventGroup;
    QString lastMessageText, lastVCardFileName, lastVCardLabel;
    int lastEventType, lastEventStatus;
};

}

using namespace CommHistory;

ContactGroupPrivate::ContactGroupPrivate(ContactGroup *parent)
    : q_ptr(parent)
    , totalMessages(0)
    , unreadMessages(0)
    , sentMessages(0)
    , lastEventId(-1)
    , lastEventGroup(0)
    , lastEventType(Event::UnknownType)
    , lastEventStatus(Event::UnknownStatus)
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
    
    startTime = endTime = lastModified = QDateTime();
    totalMessages = unreadMessages = sentMessages = 0;
    lastEventGroup = 0;

    foreach (GroupObject *group, groups) {
        foreach (const Event::Contact &contact, group->contacts())
            contacts[contact.first] = contact.second;

        if (!startTime.isValid() || group->startTime() < startTime)
            startTime = group->startTime();

        if (!endTime.isValid() || group->endTime() > endTime)
            endTime = group->endTime();

        if (group->lastEventId() >= 0 && (!lastEventGroup || group->endTime() > lastEventGroup->endTime()))
            lastEventGroup = group;

        if (!lastModified.isValid() || group->lastModified() > lastModified)
            lastModified = group->lastModified();

        totalMessages += group->totalMessages();
        unreadMessages += group->unreadMessages();
        sentMessages += group->sentMessages();
    }

    contactIds = contacts.keys();
    contactNames = contacts.values();

    if (lastEventGroup) {
        lastEventId = lastEventGroup->lastEventId();
        lastMessageText = lastEventGroup->lastMessageText();
        lastVCardFileName = lastEventGroup->lastVCardFileName();
        lastVCardLabel = lastEventGroup->lastVCardLabel();
        lastEventType = static_cast<int>(lastEventGroup->lastEventType());
        lastEventStatus = static_cast<int>(lastEventGroup->lastEventStatus());
    } else {
        lastEventId = -1;
        lastMessageText.clear();
        lastVCardFileName.clear();
        lastVCardLabel.clear();
        lastEventType = static_cast<int>(Event::UnknownType);
        lastEventStatus = static_cast<int>(Event::UnknownStatus);
    }

    emit q->contactsChanged();
    emit q->startTimeChanged();
    emit q->endTimeChanged();
    emit q->totalMessagesChanged();
    emit q->unreadMessagesChanged();
    emit q->sentMessagesChanged();
    emit q->lastEventChanged();
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

int ContactGroup::totalMessages() const
{
    Q_D(const ContactGroup);
    return d->totalMessages;
}

int ContactGroup::unreadMessages() const
{
    Q_D(const ContactGroup);
    return d->unreadMessages;
}

int ContactGroup::sentMessages() const
{
    Q_D(const ContactGroup);
    return d->sentMessages;
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

QObjectList ContactGroup::groupObjects() const
{
    Q_D(const ContactGroup);
    QObjectList l;
    l.reserve(d->groups.size());
    foreach (GroupObject *o, d->groups)
        l.append(o);
    return l;
}

bool ContactGroup::markAsRead() 
{
    Q_D(ContactGroup);

    qDebug() << Q_FUNC_INFO;
    if (d->groups.isEmpty() || !unreadMessages())
        return true;

    TrackerIO *tracker = TrackerIO::instance();
    tracker->transaction();

    foreach (GroupObject *group, d->groups) {
        if (group->unreadMessages() && !tracker->markAsReadGroup(group->id())) {
            tracker->rollback();
            return false;
        }
    }

    tracker->commit();

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

