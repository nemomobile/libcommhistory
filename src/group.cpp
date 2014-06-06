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

#include <QDBusArgument>

#include "group.h"
#include "event.h"

namespace CommHistory {

class GroupPrivate : public QSharedData
{
public:
    GroupPrivate();
    GroupPrivate(const GroupPrivate &other);
    ~GroupPrivate();

    void propertyChanged(Group::Property property) {
        validProperties += property;
        modifiedProperties += property;
    }

    int id;
    QString localUid;
    QStringList remoteUids;
    Group::ChatType chatType;
    QString chatName;
    QDateTime startTime;
    QDateTime endTime;
    int unreadMessages;
    int lastEventId;
    QList<Event::Contact> contacts;
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

GroupPrivate::GroupPrivate()
        : id(-1)
        , chatType(Group::ChatTypeP2P)
        , unreadMessages(0)
        , lastEventId(-1)
        , lastEventType(Event::UnknownType)
        , lastEventStatus(Event::UnknownStatus)
        , lastEventIsDraft(false)
{
    lastModified = QDateTime::fromTime_t(0);
}

GroupPrivate::GroupPrivate(const GroupPrivate &other)
        : QSharedData(other)
        , id(other.id)
        , localUid(other.localUid)
        , remoteUids(other.remoteUids)
        , chatType(other.chatType)
        , chatName(other.chatName)
        , startTime(other.startTime)
        , endTime(other.endTime)
        , unreadMessages(other.unreadMessages)
        , lastEventId(other.lastEventId)
        , contacts(other.contacts)
        , lastMessageText(other.lastMessageText)
        , lastVCardFileName(other.lastVCardFileName)
        , lastVCardLabel(other.lastVCardLabel)
        , lastEventType(other.lastEventType)
        , lastEventStatus(other.lastEventStatus)
        , lastEventIsDraft(other.lastEventIsDraft)
        , lastModified(other.lastModified)
        , validProperties(other.validProperties)
        , modifiedProperties(other.modifiedProperties)
{
}

GroupPrivate::~GroupPrivate()
{
}

}

using namespace CommHistory;

static Group::PropertySet setOfAllProperties;

Group::PropertySet Group::allProperties()
{
    if (setOfAllProperties.isEmpty()) {
        for (int i = 0; i < Group::NumProperties; i++)
            setOfAllProperties += (Group::Property)i;
    }

    return setOfAllProperties;
}

Group::Group()
        : d(new GroupPrivate)
{
}

Group::Group(const Group &other)
        : d(other.d)
{
}

Group &Group::operator=(const Group &other)
{
    d = other.d;
    return *this;
}

Group::~Group()
{
}

int Group::urlToId(const QString &url)
{
    if (url.startsWith(QLatin1String("conversation:")))
        return url.mid(QString(QLatin1String("conversation:")).length()).toInt();

    return -1;
}

QUrl Group::idToUrl(int id)
{
    return QUrl(QString(QLatin1String("conversation:%1")).arg(id));
}

bool Group::isValid() const
{
    return (d->id != -1);
}

Group::PropertySet Group::validProperties() const
{
    return d->validProperties;
}

Group::PropertySet Group::modifiedProperties() const
{
    return d->modifiedProperties;
}

bool Group::operator==(const Group &other) const
{
    if (this->d->id == other.id() &&
        this->d->localUid == other.localUid()) {
        return true;
    }

    return false;
}

int Group::id() const
{
    return d->id;
}


QUrl Group::url() const
{
    return Group::idToUrl(d->id);
}


QString Group::localUid() const
{
    return d->localUid;
}


QStringList Group::remoteUids() const
{
    return d->remoteUids;
}

Group::ChatType Group::chatType() const
{
    return d->chatType;
}

QString Group::chatName() const
{
    return d->chatName;
}

QDateTime Group::startTime() const
{
    return d->startTime;
}

QDateTime Group::endTime() const
{
    return d->endTime;
}

int Group::unreadMessages() const
{
    return d->unreadMessages;
}

int Group::lastEventId() const
{
    return d->lastEventId;
}

int Group::contactId() const
{
    return (!d->contacts.isEmpty() ? d->contacts.first().first : 0);
}

QString Group::contactName() const
{
    return (!d->contacts.isEmpty() ? d->contacts.first().second : QString());
}

QList<Event::Contact> Group::contacts() const
{
    return d->contacts;
}

QString Group::lastMessageText() const
{
    return d->lastMessageText;
}

QString Group::lastVCardFileName() const
{
    return d->lastVCardFileName;
}

QString Group::lastVCardLabel() const
{
    return d->lastVCardLabel;
}

Event::EventType Group::lastEventType() const
{
    return d->lastEventType;
}

Event::EventStatus Group::lastEventStatus() const
{
    return d->lastEventStatus;
}

bool Group::lastEventIsDraft() const
{
    return d->lastEventIsDraft;
}

QDateTime Group::lastModified() const
{
    return d->lastModified;
}

void Group::setValidProperties(const Group::PropertySet &properties)
{
    d->validProperties = properties;
}

void Group::resetModifiedProperties()
{
    d->modifiedProperties.clear();
}

void Group::setId(int id)
{
    d->id = id;
    d->propertyChanged(Group::Id);
}

void Group::setLocalUid(const QString &uid)
{
    d->localUid = uid;
    d->propertyChanged(Group::LocalUid);
}

void Group::setRemoteUids(const QStringList &uids)
{
    d->remoteUids = uids;
    d->propertyChanged(Group::RemoteUids);
}

void Group::setChatType(Group::ChatType chatType)
{
    d->chatType = chatType;
    d->propertyChanged(Group::Type);
}

void Group::setChatName(const QString &name)
{
    d->chatName = name;
    d->propertyChanged(Group::ChatName);
}

void Group::setStartTime(const QDateTime &startTime)
{
    d->startTime = startTime.toUTC();
    d->propertyChanged(Group::StartTime);
}

void Group::setEndTime(const QDateTime &endTime)
{
    d->endTime = endTime.toUTC();
    d->propertyChanged(Group::EndTime);
}

void Group::setUnreadMessages(int unread)
{
    d->unreadMessages = unread;
    d->propertyChanged(Group::UnreadMessages);
}

void Group::setLastEventId(int id)
{
    d->lastEventId = id;
    d->propertyChanged(Group::LastEventId);
}

void Group::setContactId(int id)
{
    if (d->contacts.isEmpty())
        d->contacts << qMakePair(id, QString());
    else
        d->contacts.first().first = id;

    d->propertyChanged(Group::Contacts);
}

void Group::setContactName(const QString &name)
{
    if (d->contacts.isEmpty())
        d->contacts << qMakePair(0, name);
    else
        d->contacts.first().second = name;

    d->propertyChanged(Group::Contacts);
}

void Group::setContacts(const QList<Event::Contact> &contacts)
{
    d->contacts = contacts;
    d->propertyChanged(Group::Contacts);
}

void Group::setLastMessageText(const QString &text)
{
    d->lastMessageText = text;
    d->propertyChanged(Group::LastMessageText);
}

void Group::setLastVCardFileName(const QString &filename)
{
    d->lastVCardFileName = filename;
    d->propertyChanged(Group::LastVCardFileName);
}

void Group::setLastVCardLabel(const QString &label)
{
    d->lastVCardLabel = label;
    d->propertyChanged(Group::LastVCardLabel);
}

void Group::setLastEventType(Event::EventType eventType)
{
    d->lastEventType = eventType;
    d->propertyChanged(Group::LastEventType);
}

void Group::setLastEventStatus(Event::EventStatus eventStatus)
{
    d->lastEventStatus = eventStatus;
    d->propertyChanged(Group::LastEventStatus);
}

void Group::setLastEventIsDraft(bool isDraft)
{
    d->lastEventIsDraft = isDraft;
    d->propertyChanged(Group::LastEventIsDraft);
}

void Group::setLastModified(const QDateTime &modified)
{
    d->lastModified = modified.toUTC();
    d->propertyChanged(Group::LastModified);
}

QDBusArgument &operator<<(QDBusArgument &argument, const Group &group)
{
    argument.beginStructure();
    argument << group.id() << group.localUid() << group.remoteUids()
             << group.chatType() << group.chatName()
             << group.endTime() << group.unreadMessages()
             << group.lastEventId() << group.contacts()
             << group.lastMessageText() << group.lastVCardFileName()
             << group.lastVCardLabel() << group.lastEventType()
             << group.lastEventStatus() << group.lastEventIsDraft()
             << group.lastModified() << group.startTime();

    // pass valid properties
    argument.beginArray(qMetaTypeId<int>());
    foreach (int e, group.validProperties())
        argument << e;
    argument.endArray();

    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Group &group)
{
    GroupPrivate p;
    int type, status;
    bool isDraft;
    uint chatType;

    argument.beginStructure();
    argument >> p.id >> p.localUid >> p.remoteUids >> chatType
             >> p.chatName >> p.endTime
             >> p.unreadMessages >> p.lastEventId >> p.contacts
             >> p.lastMessageText >> p.lastVCardFileName >> p.lastVCardLabel
             >> type >> status >> isDraft >> p.lastModified
             >> p.startTime;

    //read valid properties
    argument.beginArray();
    while (!argument.atEnd()) {
        int vp;
        argument >> vp;
        p.validProperties.insert((Group::Property)vp);
    }
    argument.endArray();

    argument.endStructure();

    if (p.validProperties.contains(Group::Id))
        group.setId(p.id);
    if (p.validProperties.contains(Group::LocalUid))
        group.setLocalUid(p.localUid);
    if (p.validProperties.contains(Group::RemoteUids))
        group.setRemoteUids(p.remoteUids);
    if (p.validProperties.contains(Group::Type))
        group.setChatType((Group::ChatType)chatType);
    if (p.validProperties.contains(Group::ChatName))
        group.setChatName(p.chatName);
    if (p.validProperties.contains(Group::EndTime))
        group.setEndTime(p.endTime);
    if (p.validProperties.contains(Group::UnreadMessages))
        group.setUnreadMessages(p.unreadMessages);
    if (p.validProperties.contains(Group::LastEventId))
        group.setLastEventId(p.lastEventId);
    if (p.validProperties.contains(Group::Contacts))
        group.setContacts(p.contacts);
    if (p.validProperties.contains(Group::LastMessageText))
        group.setLastMessageText(p.lastMessageText);
    if (p.validProperties.contains(Group::LastVCardFileName))
        group.setLastVCardFileName(p.lastVCardFileName);
    if (p.validProperties.contains(Group::LastVCardLabel))
        group.setLastVCardLabel(p.lastVCardLabel);
    if (p.validProperties.contains(Group::LastEventType))
        group.setLastEventType((Event::EventType)type);
    if (p.validProperties.contains(Group::LastEventStatus))
        group.setLastEventStatus((Event::EventStatus)status);
    if (p.validProperties.contains(Group::LastEventIsDraft))
        group.setLastEventIsDraft(isDraft);
    if (p.validProperties.contains(Group::LastModified))
        group.setLastModified(p.lastModified);
    if (p.validProperties.contains(Group::StartTime))
        group.setStartTime(p.startTime);

    group.resetModifiedProperties();

    return argument;
}

// DO NOT change this format; it is not in any way backwards compatible and will break backups
// To be replaced.
QDataStream &operator<<(QDataStream &stream, const CommHistory::Group &group)
{
    stream << group.id() << group.localUid() << group.remoteUids()
           << group.chatType() << group.chatName()
           << group.lastEventId()
           << group.lastMessageText() << group.lastVCardFileName()
           << group.lastVCardLabel() << group.lastEventType()
           << group.lastEventStatus() << group.lastModified();

    return stream;
}

QDataStream &operator>>(QDataStream &stream, CommHistory::Group &group)
{
    GroupPrivate p;
    int type, status;
    uint chatType;

    stream >> p.id >> p.localUid >> p.remoteUids >> chatType
           >> p.chatName >> p.lastEventId
           >> p.lastMessageText >> p.lastVCardFileName >> p.lastVCardLabel
           >> type >> status >> p.lastModified;

    group.setId(p.id);
    group.setLocalUid(p.localUid);
    group.setRemoteUids(p.remoteUids);
    group.setChatType((Group::ChatType)chatType);
    group.setChatName(p.chatName);
    group.setLastEventId(p.lastEventId);
    group.setLastMessageText(p.lastMessageText);
    group.setLastVCardFileName(p.lastVCardFileName);
    group.setLastVCardLabel(p.lastVCardLabel);
    group.setLastEventType((Event::EventType)type);
    group.setLastEventStatus((Event::EventStatus)status);
    group.setLastModified(p.lastModified);

    group.resetModifiedProperties();

    return stream;
}

QString Group::toString() const
{
    QString contacts;
    if (!d->contacts.isEmpty()) {
        QStringList contactList;
        foreach (Event::Contact contact, d->contacts) {
            contactList << QString("%1,%2")
                .arg(QString::number(contact.first))
                .arg(contact.second);
        }

        contacts = contactList.join(QChar(';'));
    }

    return QString("Group %1 (%2 unread) name:\"%3\" remoteUids:\"%4\" contacts:\"%5\" startTime:%6 endTime:%7")
                   .arg(d->id)
                   .arg(d->unreadMessages)
                   .arg(d->chatName)
                   .arg(d->remoteUids.join("|"))
                   .arg(contacts)
                   .arg(d->startTime.toString())
                   .arg(d->endTime.toString());
}

void Group::copyValidProperties(const Group &other)
{
    foreach(Property p, other.validProperties()) {
        switch (p) {
        case Id:
            setId(other.id());
            break;
        case LocalUid:
            setLocalUid(other.localUid());
            break;
        case RemoteUids:
            setRemoteUids(other.remoteUids());
            break;
        case Type:
            setChatType(other.chatType());
            break;
        case ChatName:
            setChatName(other.chatName());
            break;
        case EndTime:
            setEndTime(other.endTime());
            break;
        case UnreadMessages:
            setUnreadMessages(other.unreadMessages());
            break;
        case LastEventId:
            setLastEventId(other.lastEventId());
            break;
        case ContactId:
            setContactId(other.contactId());
            break;
        case ContactName:
            setContactName(other.contactName());
            break;
        case LastMessageText:
            setLastMessageText(other.lastMessageText());
            break;
        case LastVCardFileName:
            setLastVCardFileName(other.lastVCardFileName());
            break;
        case LastVCardLabel:
            setLastVCardLabel(other.lastVCardLabel());
            break;
        case LastEventType:
            setLastEventType(other.lastEventType());
            break;
        case LastEventStatus:
            setLastEventStatus(other.lastEventStatus());
            break;
        case LastEventIsDraft:
            setLastEventIsDraft(other.lastEventIsDraft());
            break;
        case LastModified:
            setLastModified(other.lastModified());
            break;
        case StartTime:
            setStartTime(other.startTime());
            break;
        case Contacts:
            setContacts(other.contacts());
            break;
        default:
            qCritical() << "Unknown group property";
            Q_ASSERT(false);
        }
    }
}
