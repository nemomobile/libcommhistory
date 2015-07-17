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
    RecipientList recipients;
    Group::ChatType chatType;
    QString chatName;
    mutable QDateTime startTime;
    mutable QDateTime endTime;
    int unreadMessages;
    int lastEventId;
    QString lastMessageText;
    QString lastVCardFileName;
    QString lastVCardLabel;
    Event::EventType lastEventType;
    Event::EventStatus lastEventStatus;
    bool lastEventIsDraft;
    mutable QDateTime lastModified;
    mutable quint32 startTimeT;
    mutable quint32 endTimeT;
    mutable quint32 lastModifiedT;

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
        , startTimeT(0)
        , endTimeT(0)
        , lastModifiedT(0)
{
}

GroupPrivate::GroupPrivate(const GroupPrivate &other)
        : QSharedData(other)
        , id(other.id)
        , localUid(other.localUid)
        , recipients(other.recipients)
        , chatType(other.chatType)
        , chatName(other.chatName)
        , unreadMessages(other.unreadMessages)
        , lastEventId(other.lastEventId)
        , lastMessageText(other.lastMessageText)
        , lastVCardFileName(other.lastVCardFileName)
        , lastVCardLabel(other.lastVCardLabel)
        , lastEventType(other.lastEventType)
        , lastEventStatus(other.lastEventStatus)
        , lastEventIsDraft(other.lastEventIsDraft)
        , startTimeT(other.startTimeT)
        , endTimeT(other.endTimeT)
        , lastModifiedT(other.lastModifiedT)
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

RecipientList Group::recipients() const
{
    return d->recipients;
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
    if (d->startTime.isNull() && d->startTimeT != 0) {
        d->startTime = QDateTime::fromTime_t(d->startTimeT);
    }
    return d->startTime;
}

QDateTime Group::endTime() const
{
    if (d->endTime.isNull() && d->endTimeT != 0) {
        d->endTime = QDateTime::fromTime_t(d->endTimeT);
    }
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
    if (d->lastModified.isNull()) {
        d->lastModified = QDateTime::fromTime_t(d->lastModifiedT);
    }
    return d->lastModified;
}

quint32 Group::startTimeT() const
{
    return d->startTimeT;
}

quint32 Group::endTimeT() const
{
    return d->endTimeT;
}

quint32 Group::lastModifiedT() const
{
    return d->lastModifiedT;
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

void Group::setRecipients(const RecipientList &r)
{
    d->recipients = r;
    d->propertyChanged(Group::Recipients);
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
    if (d->startTime.isNull()) {
        d->startTimeT = startTime.toUTC().toTime_t();
    } else {
        d->startTime = startTime.toUTC();
        d->startTimeT = d->startTime.toTime_t();
    }
    d->propertyChanged(Group::StartTime);
}

void Group::setEndTime(const QDateTime &endTime)
{
    if (d->endTime.isNull()) {
        d->endTimeT = endTime.toUTC().toTime_t();
    } else {
        d->endTime = endTime.toUTC();
        d->endTimeT = d->endTime.toTime_t();
    }
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
    if (d->lastModified.isNull()) {
        d->lastModifiedT = modified.toUTC().toTime_t();
    } else {
        d->lastModified = modified.toUTC();
        d->lastModifiedT = d->lastModified.toTime_t();
    }
    d->propertyChanged(Group::LastModified);
}

void Group::setStartTimeT(quint32 startTime)
{
    d->startTimeT = startTime;
    if (startTime == 0) {
        d->startTime = QDateTime();
    } else if (!d->startTime.isNull()) {
        d->startTime = QDateTime::fromTime_t(startTime);
    }
    d->propertyChanged(Group::StartTime);
}

void Group::setEndTimeT(quint32 endTime)
{
    d->endTimeT = endTime;
    if (endTime == 0) {
        d->endTime = QDateTime();
    } else if (!d->endTime.isNull()) {
        d->endTime = QDateTime::fromTime_t(endTime);
    }
    d->propertyChanged(Group::EndTime);
}

void Group::setLastModifiedT(quint32 modified)
{
    d->lastModifiedT = modified;
    if (!d->lastModified.isNull()) {
        d->lastModified = QDateTime::fromTime_t(d->lastModifiedT);
    }
    d->propertyChanged(Group::LastModified);
}

QDBusArgument &operator<<(QDBusArgument &argument, const Group &group)
{
    argument.beginStructure();
    argument << group.id() << group.localUid() << group.recipients()
             << group.chatType() << group.chatName()
             << group.endTimeT() << group.unreadMessages()
             << group.lastEventId() << group.lastMessageText()
             << group.lastVCardFileName() << group.lastVCardLabel()
             << group.lastEventType() << group.lastEventStatus()
             << group.lastEventIsDraft()
             << group.lastModifiedT() << group.startTimeT();

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
    argument >> p.id >> p.localUid >> p.recipients >> chatType
             >> p.chatName >> p.endTimeT
             >> p.unreadMessages >> p.lastEventId >> p.lastMessageText
             >> p.lastVCardFileName >> p.lastVCardLabel >> type
             >> status >> isDraft >> p.lastModifiedT >> p.startTimeT;

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
    if (p.validProperties.contains(Group::Recipients))
        group.setRecipients(p.recipients);
    if (p.validProperties.contains(Group::Type))
        group.setChatType((Group::ChatType)chatType);
    if (p.validProperties.contains(Group::ChatName))
        group.setChatName(p.chatName);
    if (p.validProperties.contains(Group::EndTime))
        group.setEndTimeT(p.endTimeT);
    if (p.validProperties.contains(Group::UnreadMessages))
        group.setUnreadMessages(p.unreadMessages);
    if (p.validProperties.contains(Group::LastEventId))
        group.setLastEventId(p.lastEventId);
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
        group.setLastModifiedT(p.lastModifiedT);
    if (p.validProperties.contains(Group::StartTime))
        group.setStartTimeT(p.startTimeT);

    group.resetModifiedProperties();

    return argument;
}

// DO NOT change this format; it is not in any way backwards compatible and will break backups
// To be replaced.
QDataStream &operator<<(QDataStream &stream, const CommHistory::Group &group)
{
    QStringList remoteUids;
    foreach (const Recipient &r, group.recipients())
        remoteUids.append(r.remoteUid());

    stream << group.id() << group.localUid() << remoteUids
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
    QStringList remoteUids;
    int type, status;
    uint chatType;

    stream >> p.id >> p.localUid >> remoteUids >> chatType
           >> p.chatName >> p.lastEventId
           >> p.lastMessageText >> p.lastVCardFileName >> p.lastVCardLabel
           >> type >> status >> p.lastModified;

    group.setId(p.id);
    group.setLocalUid(p.localUid);
    group.setRecipients(RecipientList::fromUids(p.localUid, remoteUids));
    group.setChatType((Group::ChatType)chatType);
    group.setChatName(p.chatName);
    group.setLastEventId(p.lastEventId);
    group.setLastMessageText(p.lastMessageText);
    group.setLastVCardFileName(p.lastVCardFileName);
    group.setLastVCardLabel(p.lastVCardLabel);
    group.setLastEventType((Event::EventType)type);
    group.setLastEventStatus((Event::EventStatus)status);
    group.setLastModifiedT(p.lastModified.toTime_t());

    group.resetModifiedProperties();

    return stream;
}

QString Group::toString() const
{
    return QString("Group %1 (%2 unread) name:\"%3\" recipients:\"%4\" startTime:%6 endTime:%7")
                   .arg(d->id)
                   .arg(d->unreadMessages)
                   .arg(d->chatName)
                   .arg(d->recipients.debugString())
                   .arg(startTime().toString())
                   .arg(endTime().toString());
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
        case Recipients:
            setRecipients(other.recipients());
            break;
        case Type:
            setChatType(other.chatType());
            break;
        case ChatName:
            setChatName(other.chatName());
            break;
        case EndTime:
            setEndTimeT(other.endTimeT());
            break;
        case UnreadMessages:
            setUnreadMessages(other.unreadMessages());
            break;
        case LastEventId:
            setLastEventId(other.lastEventId());
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
            setStartTimeT(other.startTimeT());
            break;
        default:
            qCritical() << "Unknown group property";
            Q_ASSERT(false);
        }
    }
}
