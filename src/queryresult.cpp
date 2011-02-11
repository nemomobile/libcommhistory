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

#include "group.h"

#include "queryresult.h"

using namespace CommHistory;

// used for filling data from tracker result rows
#define RESULT_INDEX(COL) result.result->value(result.columns[QLatin1String(COL)])
#define RESULT_INDEX2(COL) result->value(properties.indexOf(COL))

#define LAT(STR) QLatin1String(STR)

#define TELEPATHY_URI_PREFIX_LEN (sizeof("telepathy:") - 1)
#define IM_ADDRESS_SEPARATOR QLatin1Char('!')

#define NMO_ "http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#"

namespace {
static Event::PropertySet commonPropertySet = Event::PropertySet() << Event::StartTime
                                              << Event::EndTime
                                              << Event::LastModified
                                              << Event::IsRead
                                              << Event::Direction;
static Event::PropertySet smsOnlyPropertySet = Event::PropertySet() << Event::ParentId
                                               << Event::FromVCardFileName
                                               << Event::FromVCardLabel
                                               << Event::ValidityPeriod;

Event::EventStatus nmoStatusToEventStatus(const QString &status)
{
    if (status == LAT(NMO_ "delivery-status-sent"))
         return Event::SentStatus;
    else if (status == LAT(NMO_ "delivery-status-delivered"))
         return Event::DeliveredStatus;
    else if (status == LAT(NMO_ "delivery-status-temporarily-failed"))
        return Event::TemporarilyFailedStatus;
    else if(status == LAT(NMO_ "delivery-status-permanently-failed"))
            return Event::PermanentlyFailedStatus;

    return Event::UnknownStatus;
}
}

void QueryResult::fillEventFromModel(QueryResult &result, Event &event)
{
    Event eventToFill;
    eventToFill.setId(Event::urlToId(RESULT_INDEX("message").toString()));

    QStringList types = RESULT_INDEX("type").toString().split(QChar(','));
    if (types.contains(LAT(NMO_ "MMSMessage"))) {
        eventToFill.setType(Event::MMSEvent);
    } else if (types.contains(LAT(NMO_ "SMSMessage"))) {
        eventToFill.setType(Event::SMSEvent);
    } else if (types.contains(LAT(NMO_ "IMMessage"))) {
        eventToFill.setType(Event::IMEvent);
    } else if (types.contains(LAT(NMO_ "Call"))) {
        eventToFill.setType(Event::CallEvent);
    }

    Event::PropertySet propertyMask = result.propertyMask;

    if (eventToFill.type() == Event::CallEvent) {
        if (result.propertyMask.contains(Event::IsMissedCall))
            eventToFill.setIsMissedCall(!(RESULT_INDEX("isAnswered").toBool()));
        if (result.propertyMask.contains(Event::IsEmergencyCall))
            eventToFill.setIsEmergencyCall(RESULT_INDEX("isEmergency").toBool());
        // remove all non-call properties from set
        propertyMask &= commonPropertySet;
    }

    // handle properties common to all events
    Event::PropertySet nonSMSProperties = propertyMask - smsOnlyPropertySet;
    foreach (Event::Property property, nonSMSProperties) {
        if (property == Event::MessageToken)
            eventToFill.setMessageToken(RESULT_INDEX("messageId").toString());
        else if (property == Event::MmsId)
            eventToFill.setMmsId(RESULT_INDEX("mmsId").toString());
        else if (property == Event::IsDraft)
            eventToFill.setIsDraft(RESULT_INDEX("isDraft").toBool());
        else if (property == Event::Subject)
            eventToFill.setSubject(RESULT_INDEX("messageSubject").toString());
        else if (property == Event::FreeText)
            eventToFill.setFreeText(RESULT_INDEX("textContent").toString());
        else if (property == Event::ReportDelivery)
            eventToFill.setReportDelivery(RESULT_INDEX("reportDelivery").toBool());
        else if (property == Event::ReportRead)
            eventToFill.setReportRead(RESULT_INDEX("sentWithReportRead").toBool());
        else if (property == Event::ReportReadRequested)
            eventToFill.setReportReadRequested(RESULT_INDEX("mustAnswerReportRead").toBool());
        else if (property == Event::BytesReceived)
            eventToFill.setBytesReceived(RESULT_INDEX("contentSize").toInt());
        else if (property == Event::ContentLocation)
            eventToFill.setContentLocation(RESULT_INDEX("contentLocation").toString());
        else if (property == Event::GroupId) {
            QString channel = RESULT_INDEX("channel").toString();
            if (!channel.isEmpty())
                eventToFill.setGroupId(Group::urlToId(channel));
        } else if (property == Event::StartTime)
            eventToFill.setStartTime(RESULT_INDEX("sentDate").toDateTime().toLocalTime());
        else if (property == Event::EndTime)
            eventToFill.setEndTime(RESULT_INDEX("receivedDate").toDateTime().toLocalTime());
        else if (property == Event::IsRead)
            eventToFill.setIsRead(RESULT_INDEX("isRead").toBool());
        else if (property == Event::Status) {
            QString status = RESULT_INDEX("deliveryStatus").toString();
            if (!status.isEmpty())
                eventToFill.setStatus(nmoStatusToEventStatus(status));
        } else if (property == Event::ReadStatus) {
            QString status = RESULT_INDEX("reportReadStatus").toString();
            if (!status.isEmpty()) {
                if (status == LAT(NMO_ "read-status-read")) {
                    eventToFill.setReadStatus(Event::ReadStatusRead);
                } else if (status == LAT(NMO_ "read-status-deleted")) {
                    eventToFill.setReadStatus(Event::ReadStatusDeleted);
                } else {
                    eventToFill.setReadStatus(Event::UnknownReadStatus);
                }
            }
        } else if (property == Event::LastModified)
            eventToFill.setLastModified(RESULT_INDEX("lastModified").toDateTime().toLocalTime());
    }

    // handle SMS/MMS properties
    if (eventToFill.type() == Event::SMSEvent
        || eventToFill.type() == Event::MMSEvent) {
        if (result.propertyMask.contains(Event::ParentId))
            eventToFill.setParentId(RESULT_INDEX("smsId").toInt());

        if (result.propertyMask.contains(Event::FromVCardFileName)
            || result.propertyMask.contains(Event::FromVCardLabel)) {
            QString filename = RESULT_INDEX("fromVCardName").toString();
            if (!filename.isEmpty())
                eventToFill.setFromVCard(filename, RESULT_INDEX("fromVCardLabel").toString());
        }

        if (result.propertyMask.contains(Event::Encoding))
            eventToFill.setEncoding(RESULT_INDEX("encoding").toString());
        if (result.propertyMask.contains(Event::CharacterSet))
            eventToFill.setCharacterSet(RESULT_INDEX("characterSet").toString());
        if (result.propertyMask.contains(Event::IsDeleted))
            eventToFill.setDeleted(RESULT_INDEX("isDeleted").toBool());
        if (result.propertyMask.contains(Event::ValidityPeriod))
            eventToFill.setValidityPeriod(RESULT_INDEX("validityPeriod").toInt());
    }

    // local/remote id and direction are common to all events
    if (result.propertyMask.contains(Event::LocalUid)
        || result.propertyMask.contains(Event::RemoteUid)
        || result.propertyMask.contains(Event::Direction)) {
        if (RESULT_INDEX("isSent").toBool()) {
            eventToFill.setDirection(Event::Outbound);
        } else {
            eventToFill.setDirection(Event::Inbound);
        }

        // local contact: <telepathy:/org/.../gabble/jabber/dut_40localhost0>
        // remote contact: <telepathy:<account>!<imid>> or <tel:+35801234567>
        QString fromId = RESULT_INDEX("from").toString();
        QString toId = RESULT_INDEX("to").toString();

        if (eventToFill.direction() == Event::Outbound) {
            eventToFill.setLocalUid(fromId.mid(TELEPATHY_URI_PREFIX_LEN));
            if (toId.startsWith(LAT("tel:"))) {
                eventToFill.setRemoteUid(toId.section(QLatin1Char(':'), 1));
            } else {
                eventToFill.setRemoteUid(toId.section(IM_ADDRESS_SEPARATOR, -1));
            }
        } else {
            eventToFill.setLocalUid(toId.mid(TELEPATHY_URI_PREFIX_LEN));
            if (fromId.startsWith(LAT("tel:"))) {
                eventToFill.setRemoteUid(fromId.section(QLatin1Char(':'), 1));
            } else {
                eventToFill.setRemoteUid(fromId.section(IM_ADDRESS_SEPARATOR, -1));
            }
        }
    }

    if (eventToFill.status() == Event::UnknownStatus &&
        (eventToFill.type() == Event::SMSEvent || eventToFill.type() == Event::MMSEvent) &&
        !eventToFill.isDraft() &&
        eventToFill.direction() == Event::Outbound) {
        // treat missing status as sending for outbound SMS
        eventToFill.setStatus(Event::SendingStatus);
    }

    if (result.propertyMask.contains(Event::ContactId)
        || result.propertyMask.contains(Event::ContactName)) {
        eventToFill.setContactId(RESULT_INDEX("contactId").toInt());
        QString name = buildContactName(RESULT_INDEX("contactFirstName").toString(),
                                        RESULT_INDEX("contactLastName").toString(),
                                        RESULT_INDEX("imNickname").toString());
        eventToFill.setContactName(name);
    }

    // save data and give back as parameter
    event = eventToFill;
    event.resetModifiedProperties();
}

void QueryResult::fillEventFromModel2(Event &event)
{
    Event eventToFill;

    // handle properties common to all events
    foreach (Event::Property property, properties) {
        switch (property) {
        case Event::Id:
            eventToFill.setId(Event::urlToId(RESULT_INDEX2(Event::Id).toString()));
            break;
        case Event::Type: {
            QStringList types = RESULT_INDEX2(Event::Type).toString().split(QChar(','));
            if (types.contains(LAT(NMO_ "MMSMessage"))) {
                eventToFill.setType(Event::MMSEvent);
            } else if (types.contains(LAT(NMO_ "SMSMessage"))) {
                eventToFill.setType(Event::SMSEvent);
            } else if (types.contains(LAT(NMO_ "IMMessage"))) {
                eventToFill.setType(Event::IMEvent);
            } else if (types.contains(LAT(NMO_ "Call"))) {
                eventToFill.setType(Event::CallEvent);
            }
            break;
        }
        case Event::Direction:
            if (RESULT_INDEX2(Event::Direction).toBool()) {
                eventToFill.setDirection(Event::Outbound);
            } else {
                eventToFill.setDirection(Event::Inbound);
            }
            break;
        case Event::MessageToken:
            eventToFill.setMessageToken(RESULT_INDEX2(Event::MessageToken).toString());
            break;
        case Event::MmsId:
            eventToFill.setMmsId(RESULT_INDEX2(Event::MmsId).toString());
            break;
        case Event::IsDraft:
            eventToFill.setIsDraft(RESULT_INDEX2(Event::IsDraft).toBool());
            break;
        case Event::Subject:
            eventToFill.setSubject(RESULT_INDEX2(Event::Subject).toString());
            break;
        case Event::FreeText:
            eventToFill.setFreeText(RESULT_INDEX2(Event::FreeText).toString());
            break;
        case Event::ReportDelivery:
            eventToFill.setReportDelivery(RESULT_INDEX2(Event::ReportDelivery).toBool());
            break;
        case Event::ReportRead:
            eventToFill.setReportRead(RESULT_INDEX2(Event::ReportRead).toBool());
            break;
        case Event::ReportReadRequested:
            eventToFill.setReportReadRequested(RESULT_INDEX2(Event::ReportReadRequested).toBool());
            break;
        case Event::BytesReceived:
            eventToFill.setBytesReceived(RESULT_INDEX2(Event::BytesReceived).toInt());
            break;
        case Event::ContentLocation:
            eventToFill.setContentLocation(RESULT_INDEX2(Event::ContentLocation).toString());
            break;
        case Event::GroupId: {
            QString channel = RESULT_INDEX2(Event::GroupId).toString();
            if (!channel.isEmpty())
                eventToFill.setGroupId(Group::urlToId(channel));
            break;
        }
        case Event::StartTime:
            eventToFill.setStartTime(RESULT_INDEX2(Event::StartTime).toDateTime().toLocalTime());
            break;
        case Event::EndTime:
            eventToFill.setEndTime(RESULT_INDEX2(Event::EndTime).toDateTime().toLocalTime());
            break;
        case Event::IsRead:
            eventToFill.setIsRead(RESULT_INDEX2(Event::IsRead).toBool());
            break;
        case Event::Status: {
            QString status = RESULT_INDEX2(Event::Status).toString();
            if (!status.isEmpty())
                eventToFill.setStatus(nmoStatusToEventStatus(status));
            break;
        }
        case Event::ReadStatus: {
            QString status = RESULT_INDEX2(Event::ReadStatus).toString();
            if (!status.isEmpty()) {
                if (status == LAT(NMO_ "read-status-read")) {
                    eventToFill.setReadStatus(Event::ReadStatusRead);
                } else if (status == LAT(NMO_ "read-status-deleted")) {
                    eventToFill.setReadStatus(Event::ReadStatusDeleted);
                } else {
                    eventToFill.setReadStatus(Event::UnknownReadStatus);
                }
            }
            break;
        }
        case Event::LastModified:
            eventToFill.setLastModified(RESULT_INDEX2(Event::LastModified).toDateTime().toLocalTime());
            break;
        case Event::IsMissedCall:
            eventToFill.setIsMissedCall(!(RESULT_INDEX2(Event::IsMissedCall).toBool()));
            break;
        case Event::IsEmergencyCall:
            eventToFill.setIsEmergencyCall(RESULT_INDEX2(Event::IsEmergencyCall).toBool());
            break;
        case Event::ParentId:
            eventToFill.setParentId(RESULT_INDEX2(Event::ParentId).toInt());
            break;
        case Event::FromVCardFileName: { // handle Event::FromVCardLabel as well
            QString filename = RESULT_INDEX2(Event::FromVCardFileName).toString();
            if (!filename.isEmpty())
                eventToFill.setFromVCard(filename, RESULT_INDEX2(Event::FromVCardLabel).toString());
            break;
        }
        case Event::Encoding:
            eventToFill.setEncoding(RESULT_INDEX2(Event::Encoding).toString());
            break;
        case Event::CharacterSet:
            eventToFill.setCharacterSet(RESULT_INDEX2(Event::CharacterSet).toString());
            break;
        case Event::IsDeleted:
            eventToFill.setDeleted(RESULT_INDEX2(Event::IsDeleted).toBool());
            break;
        case Event::ValidityPeriod:
            eventToFill.setValidityPeriod(RESULT_INDEX2(Event::ValidityPeriod).toInt());
            break;
        case Event::Cc:
            eventToFill.setCcList(RESULT_INDEX2(Event::Cc).toString().split('\x1e'));
            break;
        case Event::Bcc:
            eventToFill.setBccList(RESULT_INDEX2(Event::Bcc).toString().split('\x1e'));
            break;
        case Event::To:
            eventToFill.setToList(RESULT_INDEX2(Event::To).toString().split('\x1e'));
            break;
        default:
            break;// handle below
        }
    }

    // local/remote id and direction are common to all events
    if (properties.contains(Event::LocalUid)
        || properties.contains(Event::RemoteUid)) {
        // local contact: <telepathy:/org/.../gabble/jabber/dut_40localhost0>
        // remote contact: <telepathy:<account>!<imid>> or <tel:+35801234567>
        QString fromId = RESULT_INDEX2(Event::LocalUid).toString();
        QString toId = RESULT_INDEX2(Event::RemoteUid).toString();

        if (eventToFill.direction() == Event::Outbound) {
            eventToFill.setLocalUid(fromId.mid(TELEPATHY_URI_PREFIX_LEN));
            if (toId.startsWith(LAT("tel:"))) {
                eventToFill.setRemoteUid(toId.section(QLatin1Char(':'), 1));
            } else {
                eventToFill.setRemoteUid(toId.section(IM_ADDRESS_SEPARATOR, -1));
            }
        } else {
            eventToFill.setLocalUid(toId.mid(TELEPATHY_URI_PREFIX_LEN));
            if (fromId.startsWith(LAT("tel:"))) {
                eventToFill.setRemoteUid(fromId.section(QLatin1Char(':'), 1));
            } else {
                eventToFill.setRemoteUid(fromId.section(IM_ADDRESS_SEPARATOR, -1));
            }
        }
    }

    if (eventToFill.status() == Event::UnknownStatus &&
        (eventToFill.type() == Event::SMSEvent || eventToFill.type() == Event::MMSEvent) &&
        !eventToFill.isDraft() &&
        eventToFill.direction() == Event::Outbound) {
        // treat missing status as sending for outbound SMS
        eventToFill.setStatus(Event::SendingStatus);
    }

    if (properties.contains(Event::ContactId)) {

        eventToFill.setContactId(RESULT_INDEX2(Event::ContactId).toInt());
        if (properties.contains(Event::ContactName)) {
            QString name = RESULT_INDEX2(Event::ContactName).toString();
            eventToFill.setContactName(buildContactName(name));
        }
    }

    // save data and give back as parameter
    event = eventToFill;
    event.resetModifiedProperties();
}

void QueryResult::fillGroupFromModel(QueryResult &result, Group &group)
{
    Group groupToFill;

    QStringList types = RESULT_INDEX("type").toString().split(QChar(','));
    if (types.contains(LAT(NMO_ "MMSMessage"))) {
        groupToFill.setLastEventType(Event::MMSEvent);
    } else if (types.contains(LAT(NMO_ "SMSMessage"))) {
        groupToFill.setLastEventType(Event::SMSEvent);
    } else if (types.contains(LAT(NMO_ "IMMessage"))) {
        groupToFill.setLastEventType(Event::IMEvent);
    }

    QString status = RESULT_INDEX("deliveryStatus").toString();
    if (!status.isEmpty())
        groupToFill.setLastEventStatus(nmoStatusToEventStatus(status));

    groupToFill.setId(Group::urlToId(RESULT_INDEX("channel").toString()));

    groupToFill.setChatName(RESULT_INDEX("title").toString());

    QString identifier = RESULT_INDEX("identifier").toString();
    if (!identifier.isEmpty()) {
        bool ok = false;
        Group::ChatType chatType = (Group::ChatType)(identifier.toUInt(&ok));
        if (ok)
            groupToFill.setChatType(chatType);
    }

    groupToFill.setRemoteUids(QStringList() << RESULT_INDEX("remoteId").toString());
    groupToFill.setLocalUid(RESULT_INDEX("subject").toString());
    groupToFill.setContactId(RESULT_INDEX("contactId").toInt());
    QString name = buildContactName(RESULT_INDEX("contactFirstName").toString(),
                                    RESULT_INDEX("contactLastName").toString(),
                                    RESULT_INDEX("imNickname").toString());
    groupToFill.setContactName(name);
    groupToFill.setTotalMessages(RESULT_INDEX("totalMessages").toInt());
    groupToFill.setUnreadMessages(RESULT_INDEX("unreadMessages").toInt());
    groupToFill.setSentMessages(RESULT_INDEX("sentMessages").toInt());
    groupToFill.setEndTime(RESULT_INDEX("lastDate").toDateTime().toLocalTime());
    groupToFill.setLastEventId(Event::urlToId(RESULT_INDEX("lastMessage").toString()));
    QString messageSubject = RESULT_INDEX("lastMessageSubject").toString();
    groupToFill.setLastMessageText(messageSubject.isEmpty() ? RESULT_INDEX("lastMessageText").toString() : messageSubject);
    groupToFill.setLastVCardFileName(RESULT_INDEX("lastVCardFileName").toString());
    groupToFill.setLastVCardLabel(RESULT_INDEX("lastVCardLabel").toString());

    // tracker query returns 0 for non-existing messages... make the
    // value api-compatible
    if (groupToFill.lastEventId() == 0)
        groupToFill.setLastEventId(-1);

    // we have to set nmo:sentTime for indexing, so consider time(0) as
    // invalid
    if (groupToFill.endTime() == QDateTime::fromTime_t(0))
        groupToFill.setEndTime(QDateTime());

    // since we read it from db, it is permanent
    groupToFill.setPermanent(true);

    groupToFill.setLastModified(RESULT_INDEX("lastModified").toDateTime().toLocalTime());

    group = groupToFill;
    group.resetModifiedProperties();
}

void QueryResult::fillGroupFromModel2(Group &group)
{
    Group groupToFill;

    QStringList types = result->value(Group::LastEventType).toString().split(QChar(','));
    if (types.contains(LAT(NMO_ "MMSMessage"))) {
        groupToFill.setLastEventType(Event::MMSEvent);
    } else if (types.contains(LAT(NMO_ "SMSMessage"))) {
        groupToFill.setLastEventType(Event::SMSEvent);
    } else if (types.contains(LAT(NMO_ "IMMessage"))) {
        groupToFill.setLastEventType(Event::IMEvent);
    }

    QString status = result->value(Group::LastEventStatus).toString();
    if (!status.isEmpty())
        groupToFill.setLastEventStatus(nmoStatusToEventStatus(status));

    groupToFill.setId(Group::urlToId(result->value(Group::Id).toString()));

    groupToFill.setChatName(result->value(Group::ChatName).toString());

    QString identifier = result->value(Group::Type).toString();
    if (!identifier.isEmpty()) {
        bool ok = false;
        Group::ChatType chatType = (Group::ChatType)(identifier.toUInt(&ok));
        if (ok)
            groupToFill.setChatType(chatType);
    }

    groupToFill.setRemoteUids(QStringList() << result->value(Group::RemoteUids).toString());
    groupToFill.setLocalUid(result->value(Group::LocalUid).toString());
    groupToFill.setContactId(result->value(Group::ContactId).toInt());

    QString name = buildContactName(result->value(Group::ContactName).toString());
    groupToFill.setContactName(name);

    groupToFill.setTotalMessages(result->value(Group::TotalMessages).toInt());
    groupToFill.setUnreadMessages(result->value(Group::UnreadMessages).toInt());
    groupToFill.setSentMessages(result->value(Group::SentMessages).toInt());
    groupToFill.setEndTime(result->value(Group::EndTime).toDateTime().toLocalTime());
    groupToFill.setLastEventId(Event::urlToId(result->value(Group::LastEventId).toString()));
    groupToFill.setLastMessageText(result->value(Group::LastMessageText).toString());
    groupToFill.setLastVCardFileName(result->value(Group::LastVCardFileName).toString());
    groupToFill.setLastVCardLabel(result->value(Group::LastVCardLabel).toString());

    // tracker query returns 0 for non-existing messages... make the
    // value api-compatible
    if (groupToFill.lastEventId() == 0)
        groupToFill.setLastEventId(-1);

    // we have to set nmo:sentTime for indexing, so consider time(0) as
    // invalid
    if (groupToFill.endTime() == QDateTime::fromTime_t(0))
        groupToFill.setEndTime(QDateTime());

    // since we read it from db, it is permanent
    groupToFill.setPermanent(true);

    groupToFill.setLastModified(result->value(Group::LastModified).toDateTime().toLocalTime());

    group = groupToFill;
    group.resetModifiedProperties();
}

void QueryResult::fillMessagePartFromModel(QueryResult &result,
                                         MessagePart &messagePart)
{
    MessagePart newPart;

    if (!result.eventId) {
        result.eventId = Event::urlToId(RESULT_INDEX("message").toString());
    }
    newPart.setUri(RESULT_INDEX("part").toString());
    newPart.setContentId(RESULT_INDEX("contentId").toString());
    newPart.setPlainTextContent(RESULT_INDEX("textContent").toString());
    newPart.setContentType(RESULT_INDEX("contentType").toString());
    newPart.setCharacterSet(RESULT_INDEX("characterSet").toString());
    newPart.setContentSize(RESULT_INDEX("contentSize").toInt());
    newPart.setContentLocation(RESULT_INDEX("contentLocation").toString());

    messagePart = newPart;
}

QString QueryResult::buildContactName(const QString &names)
{
    QStringList parsed = names.split('\x1e');
    if (parsed.size() == 3)
        return buildContactName(parsed[0], parsed[1], parsed[2]);

    return QString();
}

QString QueryResult::buildContactName(const QString &firstName,
                                      const QString &lastName,
                                      const QString &imNickname)
{
    if (firstName.isEmpty() && lastName.isEmpty())
        return imNickname;

    QString name = firstName;
    if (!lastName.isEmpty()) {
        if (!name.isEmpty())
            name.append(' ');
        name.append(lastName);
    }

    return name;
}


