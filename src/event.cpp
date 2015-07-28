/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jolla.com>
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

#define MMS_TO_HEADER QLatin1String("x-mms-to")
#define MMS_CC_HEADER QLatin1String("x-mms-cc")
#define MMS_BCC_HEADER QLatin1String("x-mms-bcc")
#define VIDEO_CALL_HEADER QLatin1String("x-video")

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
    mutable QDateTime startTime;
    mutable QDateTime endTime;
    int bytesReceived;

    QString localUid;
    RecipientList recipients;

    QString freeText;
    int groupId;
    QString messageToken;
    QString mmsId;

    mutable QDateTime lastModified;

    int eventCount;
    QString fromVCardFileName;
    QString fromVCardLabel;
    int validityPeriod;

    QString contentLocation;
    QString subject;
    QList<MessagePart> messageParts;

    QHash<QString, QString> headers;
    QVariantMap extraProperties;

    Event::PropertySet validProperties;
    Event::PropertySet modifiedProperties;

    mutable quint32 startTimeT;
    mutable quint32 endTimeT;
    mutable quint32 lastModifiedT;

    mutable struct {
        quint32 isDraft: 1;
        quint32 isRead: 1;
        quint32 isMissedCall: 1;
        quint32 isEmergencyCall: 1;
        quint32 isVideoCall: 1;
        quint32 isVideoCallKnown: 1;
        quint32 reportDelivery: 1;
        quint32 reportRead: 1;
        quint32 reportReadRequested: 1;
        quint32 isAction: 1;
        quint32 isResolved: 1;
        //
        quint32 type: 4;
        quint32 direction: 2;
        qint32 status: 5;
        quint32 readStatus: 2;
    } flags;
};

}

using namespace CommHistory;

static Event::PropertySet setOfAllProperties;

QDBusArgument &operator<<(QDBusArgument &argument, const Event &event)
{
    argument.beginStructure();
    argument << event.id() << event.type() << event.startTimeT()
             << event.endTimeT() << event.direction()  << event.isDraft()
             << event.isRead() << event.isMissedCall()
             << event.isEmergencyCall() << event.status()
             << event.bytesReceived() << event.localUid() << event.recipients()
             << -1 /* parentId */ << event.freeText() << event.groupId()
             << event.messageToken() << event.mmsId() << event.lastModifiedT()
             << event.eventCount()
             << event.fromVCardFileName() << event.fromVCardLabel()
             << QString() /* encoding */ << QString() /* charset */ << QString() /* language */
             << false /* isDeleted */ << event.reportDelivery()
             << event.contentLocation() << event.subject()
             << event.messageParts()
             << event.readStatus() << event.reportRead() << event.reportReadRequested()
             << event.validityPeriod() << event.isAction() << event.headers()
             << event.extraProperties();

    // pass valid properties
    argument.beginArray(qMetaTypeId<int>());
    foreach (int e, event.validProperties())
        argument << e;
    argument.endArray();

    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Event &event)
{
    EventPrivate p;
    int type, direction, status, rstatus, parentId;
    bool isDraft, isRead, isMissedCall, isEmergencyCall, reportRead, isDeleted, reportDelivery, reportReadRequested, isAction;
    QString encoding, charset, language;
    argument.beginStructure();
    argument >> p.id >> type >> p.startTimeT >> p.endTimeT
             >> direction  >> isDraft >>  isRead >> isMissedCall >> isEmergencyCall
             >> status >> p.bytesReceived >> p.localUid >> p.recipients
             >> parentId >> p.freeText >> p.groupId
             >> p.messageToken >> p.mmsId >> p.lastModifiedT >> p.eventCount
             >> p.fromVCardFileName >> p.fromVCardLabel >> encoding  >> charset >> language
             >> isDeleted >> reportDelivery >> p.contentLocation >> p.subject
             >> p.messageParts
             >> rstatus >> reportRead >> reportReadRequested
             >> p.validityPeriod >> isAction >> p.headers >> p.extraProperties;

    //read valid properties
    argument.beginArray();
    while (!argument.atEnd()) {
        int vp;
        argument >> vp;
        p.validProperties.insert((Event::Property)vp);
    }
    argument.endArray();
    argument.endStructure();

    event.setId(p.id);
    event.setType(static_cast<Event::EventType>(type));
    event.setStartTimeT(p.startTimeT);
    event.setEndTimeT(p.endTimeT);
    event.setDirection(static_cast<Event::EventDirection>(direction));
    event.setIsDraft( isDraft );
    event.setIsRead(isRead);
    event.setIsMissedCall( isMissedCall );
    event.setIsEmergencyCall( isEmergencyCall );
    event.setStatus(static_cast<Event::EventStatus>(status));
    event.setBytesReceived(p.bytesReceived);
    event.setLocalUid(p.localUid);
    event.setRecipients(p.recipients);
    event.setSubject(p.subject);
    event.setFreeText(p.freeText);
    event.setGroupId(p.groupId);
    event.setMessageToken(p.messageToken);
    event.setMmsId(p.mmsId);
    event.setLastModifiedT(p.lastModifiedT);
    event.setEventCount(p.eventCount);
    event.setFromVCard( p.fromVCardFileName, p.fromVCardLabel );
    event.setReportDelivery(reportDelivery);
    event.setValidityPeriod(p.validityPeriod);
    event.setContentLocation(p.contentLocation);
    event.setMessageParts(p.messageParts);
    event.setReadStatus((Event::EventReadStatus)rstatus);
    event.setReportRead(reportRead);
    event.setReportReadRequested(reportReadRequested);
    event.setIsAction(isAction);
    event.setHeaders(p.headers);
    event.setExtraProperties(p.extraProperties);

    event.setValidProperties(p.validProperties);
    event.resetModifiedProperties();

    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const CommHistory::Event::Contact &contact)
{
    argument.beginStructure();
    argument << contact.first << contact.second;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                CommHistory::Event::Contact &contact)
{
    argument.beginStructure();
    argument >> contact.first >> contact.second;
    argument.endStructure();
    return argument;
}

// DO NOT change this format; it is not in any way backwards compatible and will break backups
// To be replaced.
QDataStream &operator<<(QDataStream &stream, const CommHistory::Event &event)
{
    stream << event.id() << event.type() << event.startTime()
           << event.endTime() << event.direction()  << event.isDraft()
           << event.isRead() << event.isMissedCall()
           << event.isEmergencyCall() << event.status()
           << event.bytesReceived() << event.localUid()
           << event.recipients().value(0).remoteUid()
           << -1 /* parentId */ << event.freeText() << event.groupId()
           << event.messageToken() << event.mmsId() << event.lastModified()
           << event.fromVCardFileName() << event.fromVCardLabel()
           << QString() /* encoding */ << QString() /* charset */ << QString() /* language */
           << false /* isDeleted */ << event.reportDelivery()
           << event.contentLocation() << event.subject()
           << event.messageParts()
           << event.readStatus() << event.reportRead() << event.reportReadRequested()
           << event.validityPeriod() << event.isAction() << event.headers();

    return stream;
}

QDataStream &operator>>(QDataStream &stream, CommHistory::Event &event)
{
    EventPrivate p;
    int type, direction, status, rstatus, parentId;
    bool isDraft, isRead, isMissedCall, isEmergencyCall, reportRead, isDeleted, reportDelivery, reportReadRequested, isAction;
    QString encoding, charset, language;
    QString localUid, remoteUid;

    stream >> p.id >> type >> p.startTime >> p.endTime
           >> direction  >> isDraft >>  isRead >> isMissedCall >> isEmergencyCall
           >> status >> p.bytesReceived >> localUid >> remoteUid
           >> parentId >> p.freeText >> p.groupId
           >> p.messageToken >> p.mmsId >> p.lastModified
           >> p.fromVCardFileName >> p.fromVCardLabel >> encoding >> charset >> language
           >> isDeleted >> reportDelivery >> p.contentLocation >> p.subject
           >> p.messageParts
           >> rstatus >> reportRead >> reportReadRequested
           >> p.validityPeriod >> isAction >> p.headers;

    event.setId(p.id);
    event.setType(static_cast<Event::EventType>(type));
    event.setStartTimeT(p.startTime.toTime_t());
    event.setEndTimeT(p.endTime.toTime_t());
    event.setDirection(static_cast<Event::EventDirection>(direction));
    event.setIsDraft( isDraft );
    event.setIsRead(isRead);
    event.setIsMissedCall( isMissedCall );
    event.setIsEmergencyCall( isEmergencyCall );
    event.setStatus(static_cast<Event::EventStatus>(status));
    event.setBytesReceived(p.bytesReceived);
    event.setLocalUid(localUid);
    event.setRecipients(Recipient(localUid, remoteUid));
    event.setSubject(p.subject);
    event.setFreeText(p.freeText);
    event.setGroupId(p.groupId);
    event.setMessageToken(p.messageToken);
    event.setMmsId(p.mmsId);
    event.setLastModifiedT(p.lastModified.toTime_t());
    event.setFromVCard( p.fromVCardFileName, p.fromVCardLabel );
    event.setReportDelivery(reportDelivery);
    event.setValidityPeriod(p.validityPeriod);
    event.setContentLocation(p.contentLocation);
    event.setMessageParts(p.messageParts);
    event.setReadStatus((Event::EventReadStatus)rstatus);
    event.setReportRead(reportRead);
    event.setReportReadRequested(reportReadRequested);
    event.setIsAction(isAction);
    event.setHeaders(p.headers);

    event.resetModifiedProperties();

    return stream;
}

EventPrivate::EventPrivate()
        : id(-1)
        , bytesReceived(0)
        , groupId(-1)
        , eventCount(0)
        , validityPeriod(0)
        , startTimeT(0)
        , endTimeT(0)
        , lastModifiedT(0)
{
    flags.isDraft = false;
    flags.isRead = false;
    flags.isMissedCall = false;
    flags.isEmergencyCall = false;
    flags.isVideoCall = false;
    flags.isVideoCallKnown = false;
    flags.reportDelivery = false;
    flags.reportRead = false;
    flags.reportReadRequested = false;
    flags.isAction = false;
    flags.isResolved = false;

    flags.type = Event::UnknownType;
    flags.direction = Event::UnknownDirection;
    flags.status = Event::UnknownStatus;
    flags.readStatus = Event::UnknownReadStatus;
}

EventPrivate::EventPrivate(const EventPrivate &other)
        : QSharedData(other)
        , id(other.id)
        , bytesReceived(other.bytesReceived)
        , localUid(other.localUid)
        , recipients(other.recipients)
        , freeText(other.freeText)
        , groupId(other.groupId)
        , messageToken(other.messageToken)
        , mmsId(other.mmsId)
        , eventCount( other.eventCount )
        , fromVCardFileName( other.fromVCardFileName )
        , fromVCardLabel( other.fromVCardLabel )
        , validityPeriod(other.validityPeriod)
        , contentLocation(other.contentLocation)
        , subject(other.subject)
        , messageParts(other.messageParts)
        , headers(other.headers)
        , extraProperties(other.extraProperties)
        , validProperties(other.validProperties)
        , modifiedProperties(other.modifiedProperties)
        , startTimeT(other.startTimeT)
        , endTimeT(other.endTimeT)
        , lastModifiedT(other.lastModifiedT)
{
    flags.isDraft = other.flags.isDraft;
    flags.isRead = other.flags.isRead;
    flags.isMissedCall = other.flags.isMissedCall;
    flags.isEmergencyCall = other.flags.isEmergencyCall;
    flags.isVideoCall = other.flags.isVideoCall;
    flags.isVideoCallKnown = other.flags.isVideoCallKnown;
    flags.reportDelivery = other.flags.reportDelivery;
    flags.reportRead = other.flags.reportRead;
    flags.reportReadRequested = other.flags.reportReadRequested;
    flags.isAction = other.flags.isAction;
    flags.isResolved = other.flags.isResolved;

    flags.type = other.flags.type;
    flags.direction = other.flags.direction;
    flags.status = other.flags.status;
    flags.readStatus = other.flags.readStatus;
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
    return (this->d->recipients             == other.d->recipients &&
            this->d->localUid               == other.d->localUid &&
            this->d->startTimeT             == other.d->startTimeT &&
            this->d->endTimeT               == other.d->endTimeT &&
            this->d->flags.isDraft          == other.d->flags.isDraft &&
            this->d->flags.isRead           == other.d->flags.isRead &&
            this->d->flags.isMissedCall     == other.d->flags.isMissedCall &&
            this->d->flags.isEmergencyCall  == other.d->flags.isEmergencyCall &&
            this->d->flags.direction        == other.d->flags.direction &&
            this->d->id                     == other.d->id &&
            this->d->fromVCardFileName      == other.d->fromVCardFileName &&
            this->d->flags.reportDelivery   == other.d->flags.reportDelivery &&
            this->d->messageParts           == other.d->messageParts);
}

bool Event::operator!=(const Event &other) const
{
    return !(*this == other);
}

int Event::id() const
{
    return d->id;
}

QUrl Event::url() const
{
    return Event::idToUrl(d->id);
}

Event::EventType Event::type() const
{
    return static_cast<Event::EventType>(d->flags.type);
}

QDateTime Event::startTime() const
{
    if (d->startTime.isNull() && d->startTimeT != 0) {
        d->startTime = QDateTime::fromTime_t(d->startTimeT);
    }
    return d->startTime;
}

QDateTime Event::endTime() const
{
    if (d->endTime.isNull() && d->endTimeT != 0) {
        d->endTime = QDateTime::fromTime_t(d->endTimeT);
    }
    return d->endTime;
}

Event::EventDirection Event::direction() const
{
    return static_cast<Event::EventDirection>(d->flags.direction);
}

bool Event::isDraft() const
{
    return d->flags.isDraft;
}

bool Event::isRead() const
{
    return d->flags.isRead;
}

bool Event::isMissedCall() const
{
    return d->flags.isMissedCall;
}

bool Event::isEmergencyCall() const
{
    return d->flags.isEmergencyCall;
}

bool Event::isVideoCall() const
{
    if (!d->flags.isVideoCallKnown) {
        d->flags.isVideoCallKnown = true;
        d->flags.isVideoCall = false;
        QString header = d->headers.value(VIDEO_CALL_HEADER).toLower();
        if (header == QStringLiteral("true") || header == QStringLiteral("1") || header == QStringLiteral("yes"))
            d->flags.isVideoCall = true;
    }
    return d->flags.isVideoCall;
}

Event::EventStatus Event::status() const
{
    return static_cast<Event::EventStatus>(d->flags.status);
}

int Event::bytesReceived() const
{
    return d->bytesReceived;
}

QString Event::localUid() const
{
    return d->localUid;
}

const RecipientList &Event::recipients() const
{
    return d->recipients;
}

RecipientList Event::contactRecipients() const
{
    RecipientList rv;

    foreach (const Recipient &recipient, d->recipients) {
        if (recipient.contactId() != 0) {
            rv.append(recipient);
        }
    }

    return rv;
}

int Event::contactId() const
{
    return d->recipients.value(0).contactId();
}

QString Event::contactName() const
{
    return d->recipients.value(0).contactName();
}

QList<Event::Contact> Event::contacts() const
{
    QList<Event::Contact> re;
    foreach (const Recipient &r, d->recipients)
        re << qMakePair(r.contactId(), r.contactName());
    return re;
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
    if (d->lastModified.isNull()) {
        d->lastModified = QDateTime::fromTime_t(d->lastModifiedT);
    }
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
    return d->headers.value(MMS_TO_HEADER).split("\x1e", QString::SkipEmptyParts);
}

QStringList Event::ccList() const
{
    return d->headers.value(MMS_CC_HEADER).split("\x1e", QString::SkipEmptyParts);
}

QStringList Event::bccList() const
{
    return d->headers.value(MMS_BCC_HEADER).split("\x1e", QString::SkipEmptyParts);
}

Event::EventReadStatus Event::readStatus() const
{
    return static_cast<Event::EventReadStatus>(d->flags.readStatus);
}

QString Event::fromVCardFileName() const
{
    return d->fromVCardFileName;
}

QString Event::fromVCardLabel() const
{
    return d->fromVCardLabel;
}

bool Event::reportDelivery() const
{
    return d->flags.reportDelivery;
}

bool Event::reportRead() const
{
    return d->flags.reportRead;
}

bool Event::reportReadRequested() const
{
    return d->flags.reportReadRequested;
}

int Event::validityPeriod() const
{
    return d->validityPeriod;
}

QString Event::contentLocation() const
{
    return d->contentLocation;
}

bool Event::isAction() const
{
    return d->flags.isAction;
}

bool Event::isResolved() const
{
    return d->flags.isResolved;
}

QHash<QString, QString> Event::headers() const
{
    return d->headers;
}

quint32 Event::startTimeT() const
{
    return d->startTimeT;
}

quint32 Event::endTimeT() const
{
    return d->endTimeT;
}

quint32 Event::lastModifiedT() const
{
    return d->lastModifiedT;
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

void Event::setType(Event::EventType type)
{
    d->flags.type = type;
    d->propertyChanged(Event::Type);
}

void Event::setStartTime(const QDateTime &startTime)
{
    if (d->startTime.isNull()) {
        d->startTimeT = startTime.toUTC().toTime_t();
    } else {
        d->startTime = startTime.toUTC();
        d->startTimeT = d->startTime.toTime_t();
    }
    d->propertyChanged(Event::StartTime);
}

void Event::setEndTime(const QDateTime &endTime)
{
    if (d->endTime.isNull()) {
        d->endTimeT = endTime.toUTC().toTime_t();
    } else {
        d->endTime = endTime.toUTC();
        d->endTimeT = d->endTime.toTime_t();
    }
    d->propertyChanged(Event::EndTime);
}

void Event::setDirection(Event::EventDirection direction)
{
    d->flags.direction = direction;
    d->propertyChanged(Event::Direction);
}

void Event::setIsDraft( bool isDraft )
{
    d->flags.isDraft = isDraft;
    d->propertyChanged(Event::IsDraft);
}

void Event::setIsRead(bool isRead)
{
    d->flags.isRead = isRead;
    d->propertyChanged(Event::IsRead);
}

void Event::setIsMissedCall( bool isMissed )
{
    d->flags.isMissedCall = isMissed;
    d->propertyChanged(Event::IsMissedCall);
}

void Event::setIsEmergencyCall( bool isEmergency )
{
    d->flags.isEmergencyCall = isEmergency;
    d->propertyChanged(Event::IsEmergencyCall);
}

void Event::setIsVideoCall( bool isVideo )
{
    if (!isVideo) {
        d->headers.remove(VIDEO_CALL_HEADER);
    } else {
        d->headers.insert(VIDEO_CALL_HEADER, "true");
    }
    d->flags.isVideoCall = isVideo;
    d->flags.isVideoCallKnown = true;
    d->propertyChanged(Event::Headers);
}

void Event::setStatus( Event::EventStatus status )
{
    d->flags.status = status;
    d->propertyChanged(Event::Status);
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

void Event::setRecipients(const RecipientList &recipients)
{
    d->recipients = recipients;
    d->flags.isResolved = recipients.allContactsResolved();
    d->propertyChanged(Event::Recipients);
    d->propertyChanged(Event::RemoteUid);
    d->propertyChanged(Event::IsResolved);
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
    if (d->lastModified.isNull()) {
        d->lastModifiedT = modified.toUTC().toTime_t();
    } else {
        d->lastModified = modified.toUTC();
        d->lastModifiedT = d->lastModified.toTime_t();
    }
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

void Event::setReportDelivery(bool reportDelivery)
{
    d->flags.reportDelivery = reportDelivery;
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

void Event::setMessageParts(const QList<MessagePart> &parts)
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
    if (toList.isEmpty()) {
        d->headers.remove(MMS_TO_HEADER);
    } else {
        d->headers.insert(MMS_TO_HEADER, toList.join("\x1e"));
    }
    d->propertyChanged(Event::Headers);
}

void Event::setCcList(const QStringList &ccList)
{
    if (ccList.isEmpty()) {
        d->headers.remove(MMS_CC_HEADER);
    } else {
        d->headers.insert(MMS_CC_HEADER, ccList.join("\x1e"));
    }
    d->propertyChanged(Event::Headers);
}

void Event::setBccList(const QStringList &bccList)
{
    if (bccList.isEmpty()) {
        d->headers.remove(MMS_BCC_HEADER);
    } else {
        d->headers.insert(MMS_BCC_HEADER, bccList.join("\x1e"));
    }
    d->propertyChanged(Event::Headers);
}

void Event::setReadStatus(const Event::EventReadStatus eventReadStatus)
{
    d->flags.readStatus = eventReadStatus;
    d->propertyChanged(Event::ReadStatus);
}

void Event::setReportReadRequested(bool reportShouldRequested)
{
    d->flags.reportReadRequested = reportShouldRequested;
    d->propertyChanged(Event::ReportReadRequested);
}

void Event::setReportRead(bool reportRequested)
{
    d->flags.reportRead = reportRequested;
    d->propertyChanged(Event::ReportRead);
}

void Event::setIsAction(bool isAction)
{
    d->flags.isAction = isAction;
    d->propertyChanged(Event::IsAction);
}

void Event::setIsResolved(bool isResolved)
{
    d->flags.isResolved = isResolved;
    d->propertyChanged(Event::IsResolved);
}

void Event::setHeaders(const QHash<QString, QString> &headers)
{
    d->headers = headers;
    d->propertyChanged(Event::Headers);
    d->flags.isVideoCallKnown = false;
}

QVariantMap Event::extraProperties() const
{
    return d->extraProperties;
}

void Event::setExtraProperties(const QVariantMap &properties)
{
    d->extraProperties = properties;
    d->propertyChanged(Event::ExtraProperties);
}

QVariant Event::extraProperty(const QString &key) const
{
    return d->extraProperties.value(key);
}

void Event::setExtraProperty(const QString &key, const QVariant &value)
{
    if (value.isNull()) {
        if (d->extraProperties.remove(key))
            d->propertyChanged(Event::ExtraProperties);
        return;
    }

    if (!value.canConvert<QString>())
        qWarning() << "Event extra property" << key << "type cannot be converted to string:" << value;

    d->extraProperties.insert(key, value);
    d->propertyChanged(Event::ExtraProperties);
}

void Event::setStartTimeT(quint32 startTime)
{
    d->startTimeT = startTime;
    if (!d->startTime.isNull()) {
        d->startTime = QDateTime::fromTime_t(d->startTimeT);
    }
    d->propertyChanged(Event::StartTime);
}

void Event::setEndTimeT(quint32 endTime)
{
    d->endTimeT = endTime;
    if (!d->endTime.isNull()) {
        d->endTime = QDateTime::fromTime_t(d->endTimeT);
    }
    d->propertyChanged(Event::EndTime);
}

void Event::setLastModifiedT(quint32 modified)
{
    d->lastModifiedT = modified;
    if (!d->lastModified.isNull()) {
        d->lastModified = QDateTime::fromTime_t(d->lastModifiedT);
    }
    d->propertyChanged(Event::LastModified);
}

QString Event::toString() const
{
    QString headers;
    if (!d->headers.isEmpty()) {
        QStringList headerList;
        QHashIterator<QString, QString> i(d->headers);
        while (i.hasNext()) {
            i.next();
            headerList.append(QString("%1=%2").arg(i.key()).arg(i.value()));
        }

        headers = headerList.join(QChar(';'));
    }

    return QString(QString::number(id())              % QChar('|') %
                   (isEmergencyCall()
                       ? QLatin1String("!!!")
                       : QString::number(type()))     % QChar('|') %
                   startTime().toString(Qt::TextDate) % QChar('|') %
                   endTime().toString(Qt::TextDate)   % QChar('|') %
                   QString::number(direction())       % QChar('|') %
                   QString::number(isDraft())         % QChar('|') %
                   QString::number(isRead())          % QChar('|') %
                   QString::number(isMissedCall())    % QChar('|') %
                   QString::number(bytesReceived())   % QChar('|') %
                   recipients().debugString()         % QChar('|') %
                   freeText()                         % QChar('|') %
                   fromVCardFileName()                % QChar('|') %
                   fromVCardLabel()                   % QChar('|') %
                   QString::number(groupId())         % QChar('|') %
                   messageToken()                     % QChar('|') %
                   mmsId()                            % QChar('|') %
                   QString::number(eventCount())      % QChar('|') %
                   QString::number(status())          % QChar('|') %
                   QString::number(reportDelivery())  % QChar('|') %
                   QString::number(isAction())        % QChar('|') %
                   QString::number(isResolved())      % QChar('|') %
                   QString::number(messageParts().count()) % QChar('|') %
                   headers);
}

void Event::copyValidProperties(const Event &other)
{
    foreach(Property p, other.validProperties()) {
        switch (p) {
        case Id:
            setId(other.id());
            break;
        case Type:
            setType(other.type());
            break;
        case StartTime:
            setStartTimeT(other.startTimeT());
            break;
        case EndTime:
            setEndTimeT(other.endTimeT());
            break;
        case Direction:
            setDirection(other.direction());
            break;
        case IsDraft:
            setIsDraft(other.isDraft());
            break;
        case IsRead:
            setIsRead(other.isRead());
            break;
        case IsMissedCall:
            setIsMissedCall(other.isMissedCall());
            break;
        case IsEmergencyCall:
            setIsEmergencyCall(other.isEmergencyCall());
            break;
        case Status:
            setStatus(other.status());
            break;
        case BytesReceived:
            setBytesReceived(other.bytesReceived());
            break;
        case LocalUid:
            setLocalUid(other.localUid());
            break;
        case Recipients:
            setRecipients(other.recipients());
            break;
        case Subject:
            setSubject(other.subject());
            break;
        case FreeText:
            setFreeText(other.freeText());
            break;
        case GroupId:
            setGroupId(other.groupId());
            break;
        case MessageToken:
            setMessageToken(other.messageToken());
            break;
        case LastModified:
            setLastModifiedT(other.lastModifiedT());
            break;
        case EventCount:
            setEventCount(other.eventCount());
            break;
        case FromVCardFileName:
        case FromVCardLabel:
            setFromVCard(other.fromVCardFileName(), other.fromVCardLabel());
            break;
        case ReportDelivery:
            setReportDelivery(other.reportDelivery());
            break;
        case ValidityPeriod:
            setValidityPeriod(other.validityPeriod());
            break;
        case ContentLocation:
            setContentLocation(other.contentLocation());
            break;
        case MessageParts:
            setMessageParts(other.messageParts());
            break;
        case ReadStatus:
            setReadStatus(other.readStatus());
            break;
        case ReportRead:
            setReportRead(other.reportRead());
            break;
        case ReportReadRequested:
            setReportReadRequested(other.reportReadRequested());
            break;
        case MmsId:
            setMmsId(other.mmsId());
            break;
        case IsAction:
            setIsAction(other.isAction());
            break;
        case Headers:
            setHeaders(other.headers());
            break;
        case ExtraProperties:
            setExtraProperties(other.extraProperties());
            break;
        case IsResolved:
            setIsResolved(other.isResolved());
            break;
        /* Derivatives of recipients */
        case RemoteUid:
        case ContactId:
        case ContactName:
        case Contacts:
            break;
        default:
            qCritical() << "Unknown event property";
            Q_ASSERT(false);
        }
    }
}
