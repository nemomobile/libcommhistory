/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include <QDBusArgument>

#include "groupobject.h"
#include "groupmanager.h"
#include "event.h"
#include "debug.h"

namespace CommHistory {

class GroupObjectPrivate
{
    Q_DECLARE_PUBLIC(GroupObject)

public:
    GroupObject *q_ptr;
    GroupManager *manager;

    GroupObjectPrivate(GroupManager *manager, GroupObject *parent);
    virtual ~GroupObjectPrivate();

    void propertyChanged(Group::Property property);

    int id;
    QString localUid;
    RecipientList recipients;
    Group::ChatType chatType;
    QString chatName;
    QDateTime startTime;
    QDateTime endTime;
    int unreadMessages;
    int lastEventId;
    QString lastMessageText;
    QString lastVCardFileName;
    QString lastVCardLabel;
    Event::EventType lastEventType;
    Event::EventStatus lastEventStatus;
    bool lastEventIsDraft;
    QDateTime lastModified;

    Group::PropertySet validProperties;
    Group::PropertySet modifiedProperties;
};

GroupObjectPrivate::GroupObjectPrivate(GroupManager *m, GroupObject *parent)
        : q_ptr(parent)
        , manager(m)
        , id(-1)
        , chatType(Group::ChatTypeP2P)
        , unreadMessages(0)
        , lastEventId(-1)
        , lastEventType(Event::UnknownType)
        , lastEventStatus(Event::UnknownStatus)
        , lastEventIsDraft(false)
{
    Q_UNUSED(parent);
    lastModified = QDateTime::fromTime_t(0);
}

GroupObjectPrivate::~GroupObjectPrivate()
{
}

void GroupObjectPrivate::propertyChanged(Group::Property property)
{
    Q_Q(GroupObject);

    validProperties += property;
    modifiedProperties += property;

    switch (property) {
        case Group::LocalUid: emit q->localUidChanged(); break;
        case Group::Recipients: emit q->recipientsChanged(); break;
        case Group::Type: emit q->chatTypeChanged(); break;
        case Group::ChatName: emit q->chatNameChanged(); break;
        case Group::StartTime: emit q->startTimeChanged(); break;
        case Group::EndTime: emit q->endTimeChanged(); break;
        case Group::UnreadMessages: emit q->unreadMessagesChanged(); break;
        case Group::LastEventId: emit q->lastEventIdChanged(); break;
        case Group::LastMessageText: emit q->lastMessageTextChanged(); break;
        case Group::LastVCardFileName: emit q->lastVCardFileNameChanged(); break;
        case Group::LastEventType: emit q->lastEventTypeChanged(); break;
        case Group::LastEventStatus: emit q->lastEventStatusChanged(); break;
        case Group::LastEventIsDraft: emit q->lastEventIsDraftChanged(); break;
        case Group::LastModified: emit q->lastModifiedChanged(); break;
        default: break;
    }
}

}

using namespace CommHistory;

GroupObject::GroupObject(GroupManager *parent)
        : QObject(parent), d(new GroupObjectPrivate(parent, this))
{
}

GroupObject::GroupObject(const CommHistory::Group &other, GroupManager *parent)
        : QObject(parent), d(new GroupObjectPrivate(parent, this))
{
    set(other);
}

GroupObject::~GroupObject()
{
}

int GroupObject::urlToId(const QString &url)
{
    if (url.startsWith(QLatin1String("conversation:")))
        return url.mid(QString(QLatin1String("conversation:")).length()).toInt();

    return -1;
}

QUrl GroupObject::idToUrl(int id)
{
    return QUrl(QString(QLatin1String("conversation:%1")).arg(id));
}

bool GroupObject::isValid() const
{
    return (d->id != -1);
}

Group::PropertySet GroupObject::validProperties() const
{
    return d->validProperties;
}

Group::PropertySet GroupObject::modifiedProperties() const
{
    return d->modifiedProperties;
}

int GroupObject::id() const
{
    return d->id;
}

QUrl GroupObject::url() const
{
    return GroupObject::idToUrl(d->id);
}

QString GroupObject::localUid() const
{
    return d->localUid;
}

RecipientList GroupObject::recipients() const
{
    return d->recipients;
}

Group::ChatType GroupObject::chatType() const
{
    return d->chatType;
}

QString GroupObject::chatName() const
{
    return d->chatName;
}

QDateTime GroupObject::startTime() const
{
    return d->startTime;
}

QDateTime GroupObject::endTime() const
{
    return d->endTime;
}

int GroupObject::unreadMessages() const
{
    return d->unreadMessages;
}

int GroupObject::lastEventId() const
{
    return d->lastEventId;
}

QString GroupObject::lastMessageText() const
{
    return d->lastMessageText;
}

QString GroupObject::lastVCardFileName() const
{
    return d->lastVCardFileName;
}

QString GroupObject::lastVCardLabel() const
{
    return d->lastVCardLabel;
}

Event::EventType GroupObject::lastEventType() const
{
    return d->lastEventType;
}

Event::EventStatus GroupObject::lastEventStatus() const
{
    return d->lastEventStatus;
}

bool GroupObject::lastEventIsDraft() const
{
    return d->lastEventIsDraft;
}

QDateTime GroupObject::lastModified() const
{
    return d->lastModified;
}

void GroupObject::setValidProperties(const Group::PropertySet &properties)
{
    d->validProperties = properties;
}

void GroupObject::resetModifiedProperties()
{
    d->modifiedProperties.clear();
}

void GroupObject::setId(int id)
{
    d->id = id;
    d->propertyChanged(Group::Id);
}

void GroupObject::setLocalUid(const QString &uid)
{
    d->localUid = uid;
    d->propertyChanged(Group::LocalUid);
}

void GroupObject::setRecipients(const RecipientList &recipients)
{
    d->recipients = recipients;
    d->propertyChanged(Group::Recipients);
}

void GroupObject::setChatType(Group::ChatType chatType)
{
    d->chatType = chatType;
    d->propertyChanged(Group::Type);
}

void GroupObject::setChatName(const QString &name)
{
    d->chatName = name;
    d->propertyChanged(Group::ChatName);
}

void GroupObject::setStartTime(const QDateTime &startTime)
{
    d->startTime = startTime.toUTC();
    d->propertyChanged(Group::StartTime);
}

void GroupObject::setEndTime(const QDateTime &endTime)
{
    d->endTime = endTime.toUTC();
    d->propertyChanged(Group::EndTime);
}

void GroupObject::setUnreadMessages(int unread)
{
    d->unreadMessages = unread;
    d->propertyChanged(Group::UnreadMessages);
}

void GroupObject::setLastEventId(int id)
{
    d->lastEventId = id;
    d->propertyChanged(Group::LastEventId);
}

void GroupObject::setLastMessageText(const QString &text)
{
    d->lastMessageText = text;
    d->propertyChanged(Group::LastMessageText);
}

void GroupObject::setLastVCardFileName(const QString &filename)
{
    d->lastVCardFileName = filename;
    d->propertyChanged(Group::LastVCardFileName);
}

void GroupObject::setLastVCardLabel(const QString &label)
{
    d->lastVCardLabel = label;
    d->propertyChanged(Group::LastVCardLabel);
}

void GroupObject::setLastEventType(Event::EventType eventType)
{
    d->lastEventType = eventType;
    d->propertyChanged(Group::LastEventType);
}

void GroupObject::setLastEventStatus(Event::EventStatus eventStatus)
{
    d->lastEventStatus = eventStatus;
    d->propertyChanged(Group::LastEventStatus);
}

void GroupObject::setLastEventIsDraft(bool isDraft)
{
    d->lastEventIsDraft = isDraft;
    d->propertyChanged(Group::LastEventIsDraft);
}

void GroupObject::setLastModified(const QDateTime &modified)
{
    d->lastModified = modified.toUTC();
    d->propertyChanged(Group::LastModified);
}

bool GroupObject::markAsRead()
{
    if (!d->manager) {
        DEBUG() << Q_FUNC_INFO << "No manager for object instance";
        return false;
    }

    return d->manager->markAsReadGroup(id());
}

bool GroupObject::deleteGroup()
{
    if (!d->manager) {
        DEBUG() << Q_FUNC_INFO << "No manager for object instance";
        return false;
    }

    return d->manager->deleteGroups(QList<int>() << id());
}

QString GroupObject::toString() const
{
    return QString("Group %1 (%2 unread) name:\"%3\" recipients:\"%4\" startTime:%6 endTime:%7")
                   .arg(d->id)
                   .arg(d->unreadMessages)
                   .arg(d->chatName)
                   .arg(d->recipients.debugString())
                   .arg(d->startTime.toString())
                   .arg(d->endTime.toString());
}

void GroupObject::set(const Group &other)
{
    d->id = other.id();
    d->localUid = other.localUid();
    d->recipients = other.recipients();
    d->chatType = other.chatType();
    d->chatName = other.chatName();
    d->startTime = other.startTime();
    d->endTime = other.endTime();
    d->unreadMessages = other.unreadMessages();
    d->lastEventId = other.lastEventId();
    d->lastMessageText = other.lastMessageText();
    d->lastVCardFileName = other.lastVCardFileName();
    d->lastVCardLabel = other.lastVCardLabel();
    d->lastEventType = other.lastEventType();
    d->lastEventStatus = other.lastEventStatus();
    d->lastEventIsDraft = other.lastEventIsDraft();
    d->lastModified = other.lastModified();
    d->validProperties = other.validProperties();
    d->modifiedProperties = other.modifiedProperties();
}

template<typename T1, typename T2> void copyValidProperties(const T1 &from, T2 &to)
{
    foreach (Group::Property p, from.validProperties()) {
        switch (p) {
        case Group::Id:
            to.setId(from.id());
            break;
        case Group::LocalUid:
            to.setLocalUid(from.localUid());
            break;
        case Group::Recipients:
            to.setRecipients(from.recipients());
            break;
        case Group::Type:
            to.setChatType(from.chatType());
            break;
        case Group::ChatName:
            to.setChatName(from.chatName());
            break;
        case Group::EndTime:
            to.setEndTime(from.endTime());
            break;
        case Group::UnreadMessages:
            to.setUnreadMessages(from.unreadMessages());
            break;
        case Group::LastEventId:
            to.setLastEventId(from.lastEventId());
            break;
        case Group::LastMessageText:
            to.setLastMessageText(from.lastMessageText());
            break;
        case Group::LastVCardFileName:
            to.setLastVCardFileName(from.lastVCardFileName());
            break;
        case Group::LastVCardLabel:
            to.setLastVCardLabel(from.lastVCardLabel());
            break;
        case Group::LastEventType:
            to.setLastEventType(from.lastEventType());
            break;
        case Group::LastEventStatus:
            to.setLastEventStatus(from.lastEventStatus());
            break;
        case Group::LastEventIsDraft:
            to.setLastEventIsDraft(from.lastEventIsDraft());
            break;
        case Group::LastModified:
            to.setLastModified(from.lastModified());
            break;
        case Group::StartTime:
            to.setStartTime(from.startTime());
            break;
        default:
            qCritical() << "Unknown group property";
            Q_ASSERT(false);
        }
    }
}

Group GroupObject::toGroup() const
{
    Group g;
    ::copyValidProperties(*this, g);
    return g;
}

void GroupObject::copyValidProperties(const Group &other)
{
    ::copyValidProperties(other, *this);
}

