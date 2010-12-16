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

#include <QtTracker/ontologies/nco.h>
#include <QtTracker/ontologies/nmo.h>

#include "group.h"
#include "queryresult.h"

using namespace CommHistory;
using namespace SopranoLive;

// used for filling data from tracker result rows
#define RESULT_INDEX(COL) model->index(row, result.columns[COL]).data()
#define RESULT_CELL(ROW,COL) model->index(ROW,COL).data()

#define LAT(STR) QLatin1String(STR)

#define TELEPATHY_URI_PREFIX_LEN (sizeof("telepathy:") - 1)
#define IM_ADDRESS_SEPARATOR QLatin1Char('!')

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
}

void QueryResult::fillEventFromModel(QueryResult &result, int row, Event &event)
{
    Event eventToFill;

    LiveNodes model = result.model;

    eventToFill.setId(Event::urlToId(RESULT_INDEX(LAT("message")).toString()));

    QStringList types = RESULT_INDEX(LAT("type")).toString().split(QChar(','));
    if (types.contains(nmo::MMSMessage::iri().toString())) {
        eventToFill.setType(Event::MMSEvent);
    } else if (types.contains(nmo::SMSMessage::iri().toString())) {
        eventToFill.setType(Event::SMSEvent);
    } else if (types.contains(nmo::IMMessage::iri().toString())) {
        eventToFill.setType(Event::IMEvent);
    } else if (types.contains(nmo::Call::iri().toString())) {
        eventToFill.setType(Event::CallEvent);
    }

    Event::PropertySet propertyMask = result.propertyMask;

    if (eventToFill.type() == Event::CallEvent) {
        if (result.propertyMask.contains(Event::IsMissedCall))
            eventToFill.setIsMissedCall(!(RESULT_INDEX(LAT("isAnswered")).toBool()));
        // remove all non-call properties from set
        propertyMask &= commonPropertySet;
    }

    // handle properties common to all events
    Event::PropertySet nonSMSProperties = propertyMask - smsOnlyPropertySet;
    foreach (Event::Property property, nonSMSProperties) {
        if (property == Event::MessageToken)
            eventToFill.setMessageToken(RESULT_INDEX(LAT("messageId")).toString());
        else if (property == Event::MmsId)
            eventToFill.setMmsId(RESULT_INDEX(LAT("mmsId")).toString());
        else if (property == Event::IsDraft)
            eventToFill.setIsDraft(RESULT_INDEX(LAT("isDraft")).toBool());
        else if (property == Event::Subject)
            eventToFill.setSubject(RESULT_INDEX(LAT("messageSubject")).toString());
        else if (property == Event::FreeText)
            eventToFill.setFreeText(RESULT_INDEX(LAT("textContent")).toString());
        else if (property == Event::ReportDelivery)
            eventToFill.setReportDelivery(RESULT_INDEX(LAT("reportDelivery")).toBool());
        else if (property == Event::ReportRead)
            eventToFill.setReportRead(RESULT_INDEX(LAT("sentWithReportRead")).toBool());
        else if (property == Event::ReportReadRequested)
            eventToFill.setReportReadRequested(RESULT_INDEX(LAT("mustAnswerReportRead")).toBool());
        else if (property == Event::BytesReceived)
            eventToFill.setBytesReceived(RESULT_INDEX(LAT("contentSize")).toInt());
        else if (property == Event::ContentLocation)
            eventToFill.setContentLocation(RESULT_INDEX(LAT("contentLocation")).toString());
        else if (property == Event::GroupId) {
            QString channel = RESULT_INDEX(LAT("channel")).toString();
            if (!channel.isEmpty())
                eventToFill.setGroupId(Group::urlToId(channel));
        } else if (property == Event::StartTime)
            eventToFill.setStartTime(RESULT_INDEX(LAT("sentDate")).toDateTime().toLocalTime());
        else if (property == Event::EndTime)
            eventToFill.setEndTime(RESULT_INDEX(LAT("receivedDate")).toDateTime().toLocalTime());
        else if (property == Event::IsRead)
            eventToFill.setIsRead(RESULT_INDEX(LAT("isRead")).toBool());
        else if (property == Event::Status) {
            QUrl status = RESULT_INDEX(LAT("deliveryStatus")).toString();
            if (!status.isEmpty()) {
                if (status == nmo::delivery_status_sent::iri()) {
                    eventToFill.setStatus(Event::SentStatus);
                } else if (status == nmo::delivery_status_delivered::iri()) {
                    eventToFill.setStatus(Event::DeliveredStatus);
                } else if (status == nmo::delivery_status_temporarily_failed::iri()) {
                    eventToFill.setStatus(Event::TemporarilyFailedStatus);
                } else if (status == nmo::delivery_status_permanently_failed::iri()) {
                    eventToFill.setStatus(Event::PermanentlyFailedStatus);
                } else {
                    eventToFill.setStatus(Event::UnknownStatus);
                }
            }
        } else if (property == Event::ReadStatus) {
            QUrl status = RESULT_INDEX(LAT("reportReadStatus")).toString();
            if (!status.isEmpty()) {
                if (status == nmo::read_status_read::iri()) {
                    eventToFill.setReadStatus(Event::ReadStatusRead);
                } else if (status == nmo::read_status_deleted::iri()) {
                    eventToFill.setReadStatus(Event::ReadStatusDeleted);
                } else {
                    eventToFill.setReadStatus(Event::UnknownReadStatus);
                }
            }
        } else if (property == Event::LastModified)
            eventToFill.setLastModified(RESULT_INDEX(LAT("lastModified")).toDateTime().toLocalTime());
    }

    // handle SMS/MMS properties
    if (eventToFill.type() == Event::SMSEvent
        || eventToFill.type() == Event::MMSEvent) {
        if (result.propertyMask.contains(Event::ParentId))
            eventToFill.setParentId(RESULT_INDEX(LAT("smsId")).toInt());

        if (result.propertyMask.contains(Event::FromVCardFileName)
            || result.propertyMask.contains(Event::FromVCardLabel)) {
            QString filename = RESULT_INDEX(LAT("fromVCardName")).toString();
            if (!filename.isEmpty())
                eventToFill.setFromVCard(filename, RESULT_INDEX(LAT("fromVCardLabel")).toString());
        }

        if (result.propertyMask.contains(Event::Encoding))
            eventToFill.setEncoding(RESULT_INDEX(LAT("encoding")).toString());
        if (result.propertyMask.contains(Event::CharacterSet))
            eventToFill.setCharacterSet(RESULT_INDEX(LAT("characterSet")).toString());
        if (result.propertyMask.contains(Event::IsDeleted))
            eventToFill.setDeleted(RESULT_INDEX("isDeleted").toBool());
        if (result.propertyMask.contains(Event::ValidityPeriod))
            eventToFill.setValidityPeriod(RESULT_INDEX("validityPeriod").toInt());
    }

    // local/remote id and direction are common to all events
    if (result.propertyMask.contains(Event::LocalUid)
        || result.propertyMask.contains(Event::RemoteUid)
        || result.propertyMask.contains(Event::Direction)) {
        if (RESULT_INDEX(LAT("isSent")).toBool()) {
            eventToFill.setDirection(Event::Outbound);
        } else {
            eventToFill.setDirection(Event::Inbound);
        }

        // local contact: <telepathy:/org/.../gabble/jabber/dut_40localhost0>
        // remote contact: <telepathy:<account>!<imid>> or <tel:+35801234567>
        QString fromId = RESULT_INDEX(LAT("from")).toString();
        QString toId = RESULT_INDEX(LAT("to")).toString();

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
        QString name = buildContactName(RESULT_INDEX(LAT("contactFirstName")).toString(),
                                        RESULT_INDEX(LAT("contactLastName")).toString(),
                                        RESULT_INDEX(LAT("imNickname")).toString());
        eventToFill.setContactName(name);
    }

    // save data and give back as parameter
    event = eventToFill;
    event.resetModifiedProperties();
}

void QueryResult::fillGroupFromModel(QueryResult &result, int row, Group &group)
{
    Group groupToFill;
    LiveNodes model = result.model;

    QStringList types = RESULT_INDEX(LAT("type")).toString().split(QChar(','));
    if (types.contains(nmo::MMSMessage::iri().toString())) {
        groupToFill.setLastEventType(Event::MMSEvent);
    } else if (types.contains(nmo::SMSMessage::iri().toString())) {
        groupToFill.setLastEventType(Event::SMSEvent);
    } else if (types.contains(nmo::IMMessage::iri().toString())) {
        groupToFill.setLastEventType(Event::IMEvent);
    }

    QUrl status = RESULT_INDEX(LAT("deliveryStatus")).toString();
    if (!status.isEmpty()) {
        if (status == nmo::delivery_status_sent::iri()) {
            groupToFill.setLastEventStatus(Event::SentStatus);
        } else if (status == nmo::delivery_status_delivered::iri()) {
            groupToFill.setLastEventStatus(Event::DeliveredStatus);
        } else if (status == nmo::delivery_status_temporarily_failed::iri()) {
            groupToFill.setLastEventStatus(Event::TemporarilyFailedStatus);
        } else if(status == nmo::delivery_status_permanently_failed::iri()) {
            groupToFill.setLastEventStatus(Event::PermanentlyFailedStatus);
        } else {
            groupToFill.setLastEventStatus(Event::UnknownStatus);
        }
    }

    groupToFill.setId(Group::urlToId(RESULT_INDEX(LAT("channel")).toString()));

    groupToFill.setChatName(RESULT_INDEX(LAT("title")).toString());

    QString identifier = RESULT_INDEX(LAT("identifier")).toString();
    if (!identifier.isEmpty()) {
        bool ok = false;
        Group::ChatType chatType = (Group::ChatType)(identifier.toUInt(&ok));
        if (ok)
            groupToFill.setChatType(chatType);
    }

    groupToFill.setRemoteUids(QStringList() << RESULT_INDEX(LAT("remoteId")).toString());
    groupToFill.setLocalUid(RESULT_INDEX(LAT("subject")).toString());
    groupToFill.setContactId(RESULT_INDEX(LAT("contactId")).toInt());
    QString name = buildContactName(RESULT_INDEX(LAT("contactFirstName")).toString(),
                                    RESULT_INDEX(LAT("contactLastName")).toString(),
                                    RESULT_INDEX(LAT("imNickname")).toString());
    groupToFill.setContactName(name);
    groupToFill.setTotalMessages(RESULT_INDEX(LAT("totalMessages")).toInt());
    groupToFill.setUnreadMessages(RESULT_INDEX(LAT("unreadMessages")).toInt());
    groupToFill.setSentMessages(RESULT_INDEX(LAT("sentMessages")).toInt());
    groupToFill.setEndTime(RESULT_INDEX(LAT("lastDate")).toDateTime().toLocalTime());
    groupToFill.setLastEventId(Event::urlToId(RESULT_INDEX(LAT("lastMessage")).toString()));
    QString messageSubject = RESULT_INDEX(LAT("lastMessageSubject")).toString();
    groupToFill.setLastMessageText(messageSubject.isEmpty() ? RESULT_INDEX(LAT("lastMessageText")).toString() : messageSubject);
    groupToFill.setLastVCardFileName(RESULT_INDEX(LAT("lastVCardFileName")).toString());
    groupToFill.setLastVCardLabel(RESULT_INDEX(LAT("lastVCardLabel")).toString());

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

    groupToFill.setLastModified(RESULT_INDEX(LAT("lastModified")).toDateTime().toLocalTime());

    group = groupToFill;
    group.resetModifiedProperties();
}

void QueryResult::fillMessagePartFromModel(QueryResult &result, int row,
                                         MessagePart &messagePart)
{
    MessagePart newPart;
    LiveNodes model = result.model;

    if (!result.eventId) {
        result.eventId = Event::urlToId(RESULT_INDEX(LAT("message")).toString());
    }
    newPart.setUri(RESULT_INDEX(LAT("part")).toString());
    newPart.setContentId(RESULT_INDEX(LAT("contentId")).toString());
    newPart.setPlainTextContent(RESULT_INDEX(LAT("textContent")).toString());
    newPart.setContentType(RESULT_INDEX(LAT("contentType")).toString());
    newPart.setCharacterSet(RESULT_INDEX(LAT("characterSet")).toString());
    newPart.setContentSize(RESULT_INDEX(LAT("contentSize")).toInt());
    newPart.setContentLocation(RESULT_INDEX(LAT("contentLocation")).toString());

    messagePart = newPart;
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


