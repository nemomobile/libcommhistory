/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
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

#include <QDebug>
#include <QSharedDataPointer>
#include <QDBusArgument>
#include "event.h"
#include "messagepart.h"

#include <QStringBuilder>

namespace CommHistory {

class EventPrivate : public QSharedData
{
public:
    EventPrivate();
    EventPrivate(const EventPrivate &other);
    ~EventPrivate();

    void propertyChanged(Event::Property property) {
        validProperties += property;
        modifiedProperties += property;
    }

    int id;
    Event::EventType type;
    QDateTime startTime;
    QDateTime endTime;
    Event::EventDirection direction;
    bool isDraft;
    bool isRead;
    bool isMissedCall;
    bool isEmergencyCall;
    Event::EventStatus status;
    int bytesSent;
    int bytesReceived;

    QString localUid;  /* telepathy account */
    QString remoteUid;
    int contactId;
    QString contactName;
    int parentId;

    QString freeText;
    int groupId;
    QString messageToken;
    QString mmsId;

    QDateTime lastModified;

    int eventCount;
    QString fromVCardFileName;
    QString fromVCardLabel;
    QString encoding;
    QString charset;
    QString language;
    bool deleted;
    bool reportDelivery;
    bool reportRead;
    bool reportReadRequested;
    int validityPeriod;

    QString contentLocation;
    QString subject;
    QList<MessagePart> messageParts;
    QStringList toList;
    QStringList ccList;
    QStringList bccList;
    Event::EventReadStatus readStatus;

    Event::PropertySet validProperties;
    Event::PropertySet modifiedProperties;
};

}

using namespace CommHistory;

static Event::PropertySet setOfAllProperties;

QDBusArgument &operator<<(QDBusArgument &argument, const Event &event)
{
    argument.beginStructure();
    argument << event.id() << event.type() << event.startTime()
             << event.endTime() << event.direction()  << event.isDraft()
             << event.isRead() << event.isMissedCall()
             << event.isEmergencyCall() << event.status()
             << event.bytesSent() << event.bytesReceived() << event.localUid()
             << event.remoteUid() << event.contactId() << event.contactName()
             << event.parentId() << event.freeText() << event.groupId()
             << event.messageToken() << event.mmsId() << event.lastModified()
             << event.eventCount()
             << event.fromVCardFileName() << event.fromVCardLabel()
             << event.encoding() << event.characterSet() << event.language()
             << event.isDeleted() << event.reportDelivery()
             << event.contentLocation() << event.subject()
             << event.messageParts() << event.toList() << event.ccList() << event.bccList()
             << event.readStatus() << event.reportRead() << event.reportReadRequested()
             << event.validityPeriod();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Event &event)
{
    EventPrivate p;
    int type, direction, status, rstatus ;
    argument.beginStructure();
    argument >> p.id >> type >> p.startTime >> p.endTime
             >> direction  >> p.isDraft >>  p.isRead >> p.isMissedCall >> p.isEmergencyCall
             >> status >> p.bytesSent >> p.bytesReceived >> p.localUid >> p.remoteUid
             >> p.contactId >> p.contactName >> p.parentId >> p.freeText >> p.groupId
             >> p.messageToken >> p.mmsId >>p.lastModified  >> p.eventCount
             >> p.fromVCardFileName >> p.fromVCardLabel  >> p.encoding   >> p.charset >> p.language
             >> p.deleted >> p.reportDelivery >> p.contentLocation >> p.subject
             >> p.messageParts >> p.toList >> p.ccList >> p.bccList
             >> rstatus >> p.reportRead >> p.reportReadRequested
             >> p.validityPeriod;
    argument.endStructure();

    event.setId(p.id);
    event.setType((Event::EventType)type);
    event.setStartTime(p.startTime);
    event.setEndTime(p.endTime);
    event.setDirection((Event::EventDirection)direction);
    event.setIsDraft( p.isDraft );
    event.setIsRead(p.isRead);
    event.setIsMissedCall( p.isMissedCall );
    event.setIsEmergencyCall( p.isEmergencyCall );
    event.setStatus((Event::EventStatus)status);
    event.setBytesSent(p.bytesSent);
    event.setBytesReceived(p.bytesReceived);
    event.setLocalUid(p.localUid);
    event.setRemoteUid(p.remoteUid);
    event.setContactId(p.contactId);
    event.setContactName(p.contactName);
    event.setParentId(p.parentId);
    event.setSubject(p.subject);
    event.setFreeText(p.freeText);
    event.setGroupId(p.groupId);
    event.setMessageToken(p.messageToken);
    event.setMmsId(p.mmsId);
    event.setLastModified(p.lastModified);
    event.setEventCount(p.eventCount);
    event.setFromVCard( p.fromVCardFileName, p.fromVCardLabel );
    event.setEncoding(p.encoding);
    event.setCharacterSet(p.charset);
    event.setLanguage(p.language);
    event.setDeleted(p.deleted);
    event.setReportDelivery(p.reportDelivery);
    event.setValidityPeriod(p.validityPeriod);
    event.setContentLocation(p.contentLocation);
    event.setMessageParts(p.messageParts);
    event.setToList(p.toList);
    event.setCcList(p.ccList);
    event.setBccList(p.bccList);
    event.setReadStatus((Event::EventReadStatus)rstatus);
    event.setReportRead(p.reportRead);
    event.setReportReadRequested(p.reportReadRequested);

    event.resetModifiedProperties();

    return argument;
}

EventPrivate::EventPrivate()
        : id(-1)
        , type(Event::UnknownType)
        , direction(Event::UnknownDirection)
        , isDraft( false )
        , isRead(false)
        , isMissedCall( false )
        , isEmergencyCall( false )
        , status(Event::UnknownStatus)
        , bytesSent(0)
        , bytesReceived(0)
        , contactId(0)
        , parentId(-1)
        , groupId(-1)
        , eventCount( -1 )
        , deleted(false)
        , reportDelivery(false)
        , reportRead(false)
        , reportReadRequested(false)
        , readStatus(Event::UnknownReadStatus)
{
    lastModified = QDateTime::fromTime_t(0);
    validProperties += Event::Id;
}

EventPrivate::EventPrivate(const EventPrivate &other)
        : QSharedData(other)
        , id(other.id)
        , type(other.type)
        , startTime(other.startTime)
        , endTime(other.endTime)
        , direction(other.direction)
        , isDraft( other.isDraft )
        , isRead(other.isRead)
        , isMissedCall( other.isMissedCall )
        , isEmergencyCall( other.isEmergencyCall )
        , status(other.status)
        , bytesSent(other.bytesSent)
        , bytesReceived(other.bytesReceived)
        , localUid(other.localUid)
        , remoteUid(other.remoteUid)
        , contactId(other.contactId)
        , contactName(other.contactName)
        , parentId(other.parentId)
        , freeText(other.freeText)
        , groupId(other.groupId)
        , messageToken(other.messageToken)
        , mmsId(other.mmsId)
        , lastModified(other.lastModified)
        , eventCount( other.eventCount )
        , fromVCardFileName( other.fromVCardFileName )
        , fromVCardLabel( other.fromVCardLabel )
        , encoding(other.encoding)
        , charset(other.charset)
        , language(other.language)
        , deleted(other.deleted)
        , reportDelivery(other.reportDelivery)
        , reportRead(other.reportRead)
        , reportReadRequested(other.reportReadRequested)
        , validityPeriod(other.validityPeriod)
        , contentLocation(other.contentLocation)
        , subject(other.subject)
        , messageParts(other.messageParts)
        , toList(other.toList)
        , ccList(other.ccList)
        , bccList(other.bccList)
        , readStatus(other.readStatus)
        , validProperties(other.validProperties)
        , modifiedProperties(other.modifiedProperties)
{
}

EventPrivate::~EventPrivate()
{
}

Event::PropertySet Event::allProperties()
{
    if (setOfAllProperties.isEmpty()) {
        for (int i = 0; i < Event::NumProperties; i++)
            setOfAllProperties += (Event::Property)i;
    }

    return setOfAllProperties;
}

Event::Event()
    : d(new EventPrivate)
{
}

Event::Event(const Event &other)
    : d(other.d)
{
}

Event &Event::operator=(const Event &other)
{
    d = other.d;
    return *this;
}

Event::~Event()
{
}

int Event::urlToId(const QString &url)
{
    return url.mid(QString(QLatin1String("message:")).length()).toInt();
}

QUrl Event::idToUrl(int id)
{
    return QUrl(QString(QLatin1String("message:%1")).arg(id));
}

bool Event::isValid() const
{
    return (d->id != -1);
}

Event::PropertySet Event::validProperties() const
{
    return d->validProperties;
}

Event::PropertySet Event::modifiedProperties() const
{
    return d->modifiedProperties;
}

bool Event::operator==(const Event &other) const
{
    return (this->d->contactId         == other.contactId()         &&
            this->d->remoteUid         == other.remoteUid()         &&
            this->d->localUid          == other.localUid()          &&
            this->d->startTime         == other.startTime()         &&
            this->d->endTime           == other.endTime()           &&
            this->d->isDraft           == other.isDraft()           &&
            this->d->isRead            == other.isRead()            &&
            this->d->isMissedCall      == other.isMissedCall()      &&
            this->d->isEmergencyCall   == other.isEmergencyCall()   &&
            this->d->direction         == other.direction()         &&
            this->d->id                == other.id()                &&
            this->d->parentId          == other.parentId()          &&
            this->d->deleted           == other.isDeleted()         &&
            this->d->fromVCardFileName == other.fromVCardFileName() &&
            this->d->encoding          == other.encoding()          &&
            this->d->charset           == other.characterSet()      &&
            this->d->language          == other.language()          &&
            this->d->reportDelivery    == other.reportDelivery()    &&
            this->d->messageParts      == other.messageParts());
}

bool Event::operator!=(const Event &other) const
{
    return !(*this == other);
}

int Event::id() const
{
    return d->id;
}

int Event::parentId() const
{
    return d->parentId;
}

QUrl Event::url() const
{
    return Event::idToUrl(d->id);
}

Event::EventType Event::type() const
{
    return d->type;
}

QDateTime Event::startTime() const
{
    return d->startTime;
}

QDateTime Event::endTime() const
{
    return d->endTime;
}

Event::EventDirection Event::direction() const
{
    return d->direction;
}

bool Event::isDraft() const
{
    return d->isDraft;
}

bool Event::isRead() const
{
    return d->isRead;
}

bool Event::isMissedCall() const
{
    return d->isMissedCall;
}

bool Event::isEmergencyCall() const
{
    return d->isEmergencyCall;
}

Event::EventStatus Event::status() const
{
    return d->status;
}

int Event::bytesSent() const
{
    return d->bytesSent;
}

int Event::bytesReceived() const
{
    return d->bytesReceived;
}

QString Event::localUid() const
{
    return d->localUid;
}

QString Event::remoteUid() const
{
    return d->remoteUid;
}

int Event::contactId() const
{
    return d->contactId;
}

QString Event::contactName() const
{
    return d->contactName;
}

QString Event::subject() const
{
    return d->subject;
}

QString Event::freeText() const
{
    return d->freeText;
}

int Event::groupId() const
{
    return d->groupId;
}

QString Event::messageToken() const
{
    return d->messageToken;
}

QString Event::mmsId() const
{
    return d->mmsId;
}

QDateTime Event::lastModified() const
{
    return d->lastModified;
}

int Event::eventCount() const
{
    return d->eventCount;
}

QList<MessagePart> Event::messageParts() const
{
    return d->messageParts;
}

QStringList Event::toList() const
{
    return d->toList;
}

QStringList Event::ccList() const
{
    return d->ccList;
}

QStringList Event::bccList() const
{
    return d->bccList;
}

Event::EventReadStatus Event::readStatus() const
{
    return d->readStatus;
}

QString Event::fromVCardFileName() const
{
    return d->fromVCardFileName;
}

QString Event::fromVCardLabel() const
{
    return d->fromVCardLabel;
}

QString Event::characterSet() const
{
    return d->charset;
}

QString Event::language() const
{
    return d->language;
}

QString Event::encoding() const
{
    return d->encoding;
}

bool Event::isDeleted() const
{
    return d->deleted;
}

bool Event::reportDelivery() const
{
    return d->reportDelivery;
}

bool Event::reportRead() const
{
    return d->reportRead;
}

bool Event::reportReadRequested() const
{
    return d->reportReadRequested;
}

int Event::validityPeriod() const
{
    return d->validityPeriod;
}

QString Event::contentLocation() const
{
    return d->contentLocation;
}

void Event::setValidProperties(const Event::PropertySet &properties)
{
    d->validProperties = properties;
}

void Event::resetModifiedProperties()
{
    d->modifiedProperties.clear();
}

bool Event::resetModifiedProperty(Event::Property property)
{
    return d->modifiedProperties.remove(property);
}

void Event::setId(int id)
{
    d->id = id;
    d->propertyChanged(Event::Id);
}

void Event::setParentId(int id)
{
    d->parentId = id;
    d->propertyChanged(Event::ParentId);
}

void Event::setType(Event::EventType type)
{
    d->type = type;
    d->propertyChanged(Event::Type);
}

void Event::setStartTime(const QDateTime &startTime)
{
    d->startTime = startTime;
    d->propertyChanged(Event::StartTime);
}

void Event::setEndTime(const QDateTime &endTime)
{
    d->endTime = endTime;
    d->propertyChanged(Event::EndTime);
}

void Event::setDirection(Event::EventDirection direction)
{
    d->direction = direction;
    d->propertyChanged(Event::Direction);
}

void Event::setIsDraft( bool isDraft )
{
    d->isDraft = isDraft;
    d->propertyChanged(Event::IsDraft);
}

void Event::setIsRead(bool isRead)
{
    d->isRead = isRead;
    d->propertyChanged(Event::IsRead);
}

void Event::setIsMissedCall( bool isMissed )
{
    d->isMissedCall = isMissed;
    d->propertyChanged(Event::IsMissedCall);
}

void Event::setIsEmergencyCall( bool isEmergency )
{
    d->isEmergencyCall = isEmergency;
    d->validProperties += Event::IsEmergencyCall;
}

void Event::setStatus( Event::EventStatus status )
{
    d->status = status;
    d->propertyChanged(Event::Status);
}

void Event::setBytesSent(int bytes)
{
    d->bytesSent = bytes;
    d->propertyChanged(Event::BytesSent);
}

void Event::setBytesReceived(int bytes)
{
    d->bytesReceived = bytes;
    d->propertyChanged(Event::BytesReceived);
}

void Event::setLocalUid(const QString &uid)
{
    d->localUid = uid;
    d->propertyChanged(Event::LocalUid);
}

void Event::setRemoteUid(const QString &uid)
{
    d->remoteUid = uid;
    d->propertyChanged(Event::RemoteUid);
}

void Event::setContactId(int id)
{
    d->contactId = id;
    d->propertyChanged(Event::ContactId);
}

void Event::setContactName(const QString &name)
{
    d->contactName = name;
    d->propertyChanged(Event::ContactName);
}

void Event::setSubject(const QString &subject)
{
    d->subject = subject;
    d->propertyChanged(Event::Subject);
}

void Event::setFreeText(const QString &text)
{
    d->freeText = text;
    d->propertyChanged(Event::FreeText);
}

void Event::setGroupId(int id)
{
    d->groupId = id;
    d->propertyChanged(Event::GroupId);
}

void Event::setMessageToken(const QString &messageToken)
{
    d->messageToken = messageToken;
    d->propertyChanged(Event::MessageToken);
}

void Event::setMmsId(const QString &mmsId)
{
    d->mmsId = mmsId;
    d->propertyChanged(Event::MmsId);
}

void Event::setLastModified(const QDateTime &modified)
{
    d->lastModified = modified;
    d->propertyChanged(Event::LastModified);
}

void Event::setEventCount( int count )
{
    d->eventCount = count;
    d->propertyChanged(Event::EventCount);
}

void Event::setFromVCard( const QString &filename, const QString &label )
{
    d->fromVCardFileName = filename;
    d->fromVCardLabel = label.isEmpty() ? filename : label;
    d->propertyChanged(Event::FromVCardFileName);
    d->propertyChanged(Event::FromVCardLabel);
}

void Event::setEncoding(const QString& enc)
{
    d->encoding = enc;
    d->propertyChanged(Event::Encoding);
}

void Event::setCharacterSet(const QString& charset)
{
    d->charset = charset;
    d->propertyChanged(Event::CharacterSet);
}

void Event::setLanguage(const QString &lang)
{
    d->language = lang;
    d->propertyChanged(Event::Language);
}

void Event::setDeleted(bool isDel)
{
    d->deleted = isDel;
    d->propertyChanged(Event::IsDeleted);
}

void Event::setReportDelivery(bool reportDelivery)
{
    d->reportDelivery = reportDelivery;
    d->propertyChanged(Event::ReportDelivery);
}

void Event::setValidityPeriod(int validity)
{
    d->validityPeriod = validity;
    d->propertyChanged(Event::ValidityPeriod);
}

void Event::setContentLocation(const QString &location)
{
    d->contentLocation = location;
    d->propertyChanged(Event::ContentLocation);
}

void Event::setMessageParts(QList<MessagePart> &parts)
{
    d->messageParts = parts;
    d->propertyChanged(Event::MessageParts);
}

void Event::addMessagePart(const MessagePart &part)
{
    d->messageParts.append(part);
    d->propertyChanged(Event::MessageParts);
}

void Event::setToList(const QStringList &toList)
{
    d->toList = toList;
    d->propertyChanged(Event::To);
}

void Event::setCcList(const QStringList &ccList)
{
    d->ccList = ccList;
    d->propertyChanged(Event::Cc);
}

void Event::setBccList(const QStringList &bccList)
{
    d->bccList = bccList;
    d->propertyChanged(Event::Bcc);
}

void Event::setReadStatus(const Event::EventReadStatus eventReadStatus)
{
    d->readStatus = eventReadStatus;
    d->propertyChanged(Event::ReadStatus);
}

void Event::setReportReadRequested(bool reportShouldRequested)
{
    d->reportReadRequested = reportShouldRequested;
    d->propertyChanged(Event::ReportReadRequested);
}

void Event::setReportRead(bool reportRequested)
{
    d->reportRead = reportRequested;
    d->propertyChanged(Event::ReportRead);
}

QString Event::toString() const
{
    return QString(QString::number(id())              % QChar('|') %
                   (isEmergencyCall() ? QLatin1String("!!!") :
                    QString::number(type()))          % QChar('|') %
                   startTime().toString(Qt::TextDate) % QChar('|') %
                   endTime().toString(Qt::TextDate)   % QChar('|') %
                   QString::number(direction())       % QChar('|') %
                   QString::number(isDraft())         % QChar('|') %
                   QString::number(isRead())          % QChar('|') %
                   QString::number(isMissedCall())    % QChar('|') %
                   QString::number(bytesSent())       % QChar('|') %
                   QString::number(bytesReceived())   % QChar('|') %
                   localUid()                         % QChar('|') %
                   remoteUid()                        % QChar('|') %
                   QString::number(contactId())       % QChar('|') %
                   contactName()                      % QChar('|') %
                   freeText()                         % QChar('|') %
                   fromVCardFileName()                % QChar('|') %
                   fromVCardLabel()                   % QChar('|') %
                   QString::number(groupId())         % QChar('|') %
                   messageToken()                     % QChar('|') %
                   mmsId()                            % QChar('|') %
                   QString::number(eventCount())      % QChar('|') %
                   QString::number(status())          % QChar('|') %
                   QString::number(reportDelivery())  % QChar('|') %
                   QString::number(messageParts().count()));
}
