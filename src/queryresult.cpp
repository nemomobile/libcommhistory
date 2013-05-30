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

#include "group.h"

#include "queryresult.h"
#include "contactlistener.h"
#include "commonutils.h"
#include "qcontacttpmetadata_p.h"

#include <QContact>
#include <QContactManager>
#include <QContactDetail>
#include <QContactDetailFilter>
#include <QContactIntersectionFilter>
#include <QContactUnionFilter>

#include <QContactName>
#include <QContactNickname>
#include <QContactOnlineAccount>
#include <QContactPhoneNumber>
#include <QContactPresence>

#include <QSettings>
using namespace CommHistory;

// used for filling data from tracker result rows
#define RESULT_INDEX(COL) result.result->value(result.columns[QLatin1String(COL)])
#define RESULT_INDEX2(COL) result->value(properties.indexOf(COL))

#define LAT(STR) QLatin1String(STR)

#define TELEPATHY_URI_PREFIX_LEN (sizeof("telepathy:") - 1)
#define IM_ADDRESS_SEPARATOR QLatin1Char('!')

#define NMO_ "http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
void QContactTpMetadata::setContactId(const QString &s) { setValue(FieldContactId, s); }
QString QContactTpMetadata::contactId() const { return value(FieldContactId); }

void QContactTpMetadata::setAccountId(const QString &s) { setValue(FieldAccountId, s); }
QString QContactTpMetadata::accountId() const { return value(FieldAccountId); }

void QContactTpMetadata::setAccountEnabled(bool b) { setValue(FieldAccountEnabled, QLatin1String(b ? "true" : "false")); }
bool QContactTpMetadata::accountEnabled() const { return (value(FieldAccountEnabled) == QLatin1String("true")); }

QContactDetailFilter QContactTpMetadata::matchContactId(const QString &s)
{
    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactTpMetadata::DefinitionName, FieldContactId);
    filter.setValue(s);
    filter.setMatchFlags(QContactFilter::MatchExactly);
    return filter;
}

QContactDetailFilter QContactTpMetadata::matchAccountId(const QString &s)
{
    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactTpMetadata::DefinitionName, FieldAccountId);
    filter.setValue(s);
    filter.setMatchFlags(QContactFilter::MatchExactly);
    return filter;
}

Q_IMPLEMENT_CUSTOM_CONTACT_DETAIL(QContactTpMetadata, "TpMetadata");
Q_DEFINE_LATIN1_CONSTANT(QContactTpMetadata::FieldContactId, "ContactId");
Q_DEFINE_LATIN1_CONSTANT(QContactTpMetadata::FieldAccountId, "AccountId");
Q_DEFINE_LATIN1_CONSTANT(QContactTpMetadata::FieldAccountEnabled, "AccountEnabled");
#endif

namespace {

Event::EventStatus nmoStatusToEventStatus(const QString &status)
{
    if (status == LAT(NMO_ "delivery-status-sent"))
         return Event::SentStatus;
    else if (status == LAT(NMO_ "delivery-status-delivered"))
         return Event::DeliveredStatus;
    else if (status == LAT(NMO_ "delivery-status-temporarily-failed"))
        return Event::TemporarilyFailedStatus;
    else if (status == LAT(NMO_ "delivery-status-temporarily-failed-offline"))
        return Event::TemporarilyFailedOfflineStatus;
    else if(status == LAT(NMO_ "delivery-status-permanently-failed"))
            return Event::PermanentlyFailedStatus;

    return Event::UnknownStatus;
}

// parse concatted & coalesced sip/tel/IM remote id column
QString parseRemoteUid(const QString &remoteUid)
{
    QString result;

    QStringList uids = remoteUid.split('\x1e', QString::SkipEmptyParts);
    foreach (QString id, uids) {
        if (id.startsWith(LAT("sip:")) || id.startsWith(LAT("sips:")))
            return id;
    }

    if (!uids.isEmpty())
        result = uids.first().section(IM_ADDRESS_SEPARATOR, -1);

    return result;
}

QString getAddresbookNameOrder()
{
    QSettings addressBookSettings(QSettings::IniFormat, QSettings::UserScope,
                                  LAT("Nokia"), LAT("Contacts"));
    return addressBookSettings.value(LAT("nameOrder")).toString();
}

QContactManager *createManager()
{
    QString envspec(QLatin1String(qgetenv("NEMO_CONTACT_MANAGER")));
    if (!envspec.isEmpty()) {
        qDebug() << "Using contact manager:" << envspec;
        return new QContactManager(envspec);
    }

    return new QContactManager;
}

QContactManager *manager()
{
    static QContactManager *manager = createManager();
    return manager;
}

QContactFilter matchIMAddressFilter(const QString &localUid, const QString &imAddress)
{
    Q_UNUSED(localUid);
    Q_UNUSED(imAddress);
    QContactDetailFilter filter;
    // QT5: Add telepathy details from contacts
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    filter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName, QContactOnlineAccount::FieldAccountUri);
    filter.setMatchFlags(QContactFilter::MatchExactly);

    if (imAddress.startsWith(QLatin1String("telepathy:"))) {
        // Only match contacts created by telepathy for the specified account
        // Note: qtcontacts-sqlite uses the QContactTpMetadata detail to reflect the telepathy
        // origin, so the 'telepathy:' qualification must be removed from the values
        QContactIntersectionFilter tpFilter;
        tpFilter << QContactTpMetadata::matchAccountId(localUid);

        QString trailing(imAddress);
        trailing.remove(0, 10);
        filter.setValue(trailing);

        tpFilter << filter;
        return tpFilter;
    }

    filter.setValue(imAddress);
#endif
    return filter;
}

QContactUnionFilter matchContactFilter(const QString &localUid, const QString &match)
{
    QContactUnionFilter filter;
    filter << QContactPhoneNumber::match(match);
    filter << matchIMAddressFilter(localUid, match);

    return filter;
}

QContactFetchHint getRetrievalHint()
{
    QContactFetchHint hint;

    // Only retrieve the details we will use
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    hint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactDetail::TypeName
                                                                << QContactDetail::TypePhoneNumber
                                                                << QContactDetail::TypeOnlineAccount
                                                                << QContactDetail::TypeNickname
                                                                << QContactDetail::TypePresence);
#else
    hint.setDetailDefinitionsHint(QStringList() << QContactName::DefinitionName
                                                << QContactPhoneNumber::DefinitionName
                                                << QContactOnlineAccount::DefinitionName
                                                << QContactNickname::DefinitionName
                                                << QContactPresence::DefinitionName);
#endif

    // Relationships are slow and unnecessary here
    hint.setOptimizationHints(QContactFetchHint::NoRelationships);

    return hint;
}

QList<QContact> findMatchingFromList(QList<QContact> *list, const QString &match)
{
    QList<QContact> rv;

    QString shortMatch = makeShortNumber(match, NormalizeFlagKeepDialString);
    QString accountUri = match;
    if (accountUri.startsWith(QLatin1String("telepathy:"))) {
        accountUri.remove(0, 10);
    }

    for (QList<QContact>::iterator it = list->begin(); it != list->end(); ) {
        const QContact &contact(*it);

        // Is this contact a match for this criterion?
        QList<QContact>::iterator matchIt = list->end();
        foreach (const QContactPhoneNumber &number, contact.details<QContactPhoneNumber>()) {
            QString shortForm = makeShortNumber(number.number(), NormalizeFlagKeepDialString);
            if (shortForm == shortMatch) {
                matchIt = it;
                break;
            }
        }
        foreach (const QContactOnlineAccount &account, contact.details<QContactOnlineAccount>()) {
            if (account.accountUri() == accountUri) {
                matchIt = it;
                break;
            }
        }

        if (matchIt != list->end()) {
            rv.append(*matchIt);
            // Don't match this contact again
            it = list->erase(matchIt);
        } else {
            ++it;
        }
    }

    return rv;
}

QHash<QString, QList<QContact> > findMatchingContacts(const QString &localUid, const QStringList &matches)
{
    static const QContactFetchHint retrievalHint(getRetrievalHint());
    static const QList<QContactSortOrder> defaultOrder;

    QHash<QString, QList<QContact> > rv;

    // Fetch all matches in one query
    QContactUnionFilter groupFilter;
    foreach (const QString &match, matches) {
        groupFilter << matchContactFilter(localUid, match);
    }

    QList<QContact> matchList = manager()->contacts(groupFilter, defaultOrder, retrievalHint);
    foreach (const QString &match, matches) {
        // For each remoteUid, find the returned match(es), so that contacts order can be preserved
        rv.insert(match, findMatchingFromList(&matchList, match));
    }

    return rv;
}

}

void QueryResult::fillEventFromModel(Event &event, ContactMap *contactMap)
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
            eventToFill.setStartTime(RESULT_INDEX2(Event::StartTime).toDateTime());
            break;
        case Event::EndTime:
            eventToFill.setEndTime(RESULT_INDEX2(Event::EndTime).toDateTime());
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
            eventToFill.setLastModified(RESULT_INDEX2(Event::LastModified).toDateTime());
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
            eventToFill.setCcList(RESULT_INDEX2(Event::Cc).toString().split('\x1e', QString::SkipEmptyParts));
            break;
        case Event::Bcc:
            eventToFill.setBccList(RESULT_INDEX2(Event::Bcc).toString().split('\x1e', QString::SkipEmptyParts));
            break;
        case Event::To:
        case Event::Headers: {
            QHash<QString, QString> headers;
            parseHeaders(RESULT_INDEX2(Event::Headers).toString(), headers);
            eventToFill.setHeaders(headers);
            break;
        }
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
            eventToFill.setRemoteUid(parseRemoteUid(toId));
        } else {
            eventToFill.setLocalUid(toId.mid(TELEPATHY_URI_PREFIX_LEN));
            eventToFill.setRemoteUid(parseRemoteUid(fromId));
        }
    }

    if (eventToFill.status() == Event::UnknownStatus &&
        (eventToFill.type() == Event::SMSEvent || eventToFill.type() == Event::MMSEvent) &&
        !eventToFill.isDraft() &&
        eventToFill.direction() == Event::Outbound) {
        // treat missing status as sending for outbound SMS
        eventToFill.setStatus(Event::SendingStatus);
    }

    if (eventToFill.type() == Event::IMEvent) {
        eventToFill.setIsAction(RESULT_INDEX2(Event::IsAction).toBool());
    }

    // TODO: what to do with the contact id and nickname columns if
    // Event::ContactId and Event::ContactName are replaced with
    // Event::Contacts?
    if (properties.contains(Event::ContactId)) {
        QList<Event::Contact> contacts;
        parseContacts(RESULT_INDEX2(Event::ContactId).toString(),
                      eventToFill.localUid(), contacts, contactMap);
        eventToFill.setContacts(contacts);
    }

    // save data and give back as parameter
    event = eventToFill;
    event.resetModifiedProperties();
}

void QueryResult::fillGroupFromModel(Group &group, ContactMap *contactMap)
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

    QList<Event::Contact> contacts;
    parseContacts(result->value(Group::ContactId).toString(),
                  groupToFill.localUid(), contacts, contactMap);
    groupToFill.setContacts(contacts);

    groupToFill.setTotalMessages(result->value(Group::TotalMessages).toInt());
    groupToFill.setUnreadMessages(result->value(Group::UnreadMessages).toInt());
    groupToFill.setSentMessages(result->value(Group::SentMessages).toInt());
    groupToFill.setEndTime(result->value(Group::EndTime).toDateTime());
    groupToFill.setLastEventId(Event::urlToId(result->value(Group::LastEventId).toString()));
    groupToFill.setLastVCardFileName(result->value(Group::LastVCardFileName).toString());
    groupToFill.setLastVCardLabel(result->value(Group::LastVCardLabel).toString());

    QStringList text = result->value(Group::LastMessageText).toString().split("\x1e", QString::SkipEmptyParts);
    if (!text.isEmpty())
        groupToFill.setLastMessageText(text[0]);

    // tracker query returns 0 for non-existing messages... make the
    // value api-compatible
    if (groupToFill.lastEventId() == 0)
        groupToFill.setLastEventId(-1);

    // we have to set nmo:sentTime for indexing, so consider time(0) as
    // invalid
    if (groupToFill.endTime() == QDateTime::fromTime_t(0))
        groupToFill.setEndTime(QDateTime());

    groupToFill.setLastModified(result->value(Group::LastModified).toDateTime());
    groupToFill.setStartTime(result->value(Group::StartTime).toDateTime());

    group = groupToFill;
    group.resetModifiedProperties();
}

void QueryResult::fillMessagePartFromModel(MessagePart &messagePart)
{
    MessagePart newPart;

    if (!eventId) {
        eventId = Event::urlToId(result->value(MessagePartColumnMessage).toString());
    }
    newPart.setUri(result->value(MessagePartColumnMessagePart).toString());
    newPart.setContentId(result->value(MessagePartColumnContentId).toString());
    newPart.setPlainTextContent(result->value(MessagePartColumnText).toString());
    newPart.setContentType(result->value(MessagePartColumnMimeType).toString());
    newPart.setCharacterSet(result->value(MessagePartColumnCharacterSet).toString());
    newPart.setContentSize(result->value(MessagePartColumnContentSize).toInt());
    newPart.setContentLocation(result->value(MessagePartColumnFileName).toString());

    messagePart = newPart;
}

void QueryResult::fillCallGroupFromModel(Event &event, ContactMap *contactMap)
{
    Event eventToFill;

    eventToFill.setType(Event::CallEvent);
    eventToFill.setId(Event::urlToId(result->value(CallGroupColumnLastCall).toString()));
    eventToFill.setStartTime(result->value(CallGroupColumnStartTime).toDateTime().toLocalTime());
    eventToFill.setEndTime(result->value(CallGroupColumnEndTime).toDateTime().toLocalTime());
    QString fromId = result->value(CallGroupColumnFrom).toString();
    QString toId = result->value(CallGroupColumnTo).toString();

    if (result->value(CallGroupColumnIsSent).toBool()) {
        eventToFill.setDirection(Event::Outbound);
        eventToFill.setLocalUid(fromId.mid(TELEPATHY_URI_PREFIX_LEN));
        eventToFill.setRemoteUid(parseRemoteUid(toId));
    } else {
        eventToFill.setDirection(Event::Inbound);
        eventToFill.setLocalUid(toId.mid(TELEPATHY_URI_PREFIX_LEN));
        eventToFill.setRemoteUid(parseRemoteUid(fromId));
    }

    eventToFill.setIsMissedCall(!(result->value(CallGroupColumnIsAnswered).toBool()));
    eventToFill.setIsEmergencyCall(result->value(CallGroupColumnIsEmergency).toBool());
    eventToFill.setIsRead(result->value(CallGroupColumnIsRead).toBool());
    eventToFill.setLastModified(result->value(CallGroupColumnLastModified).toDateTime().toLocalTime());

    QList<Event::Contact> contacts;
    parseContacts(result->value(CallGroupColumnContacts).toString(),
                  eventToFill.localUid(), contacts, contactMap);
    eventToFill.setContacts(contacts);

    eventToFill.setEventCount(result->value(CallGroupColumnMissedCount).toInt());

    if (result->value(CallGroupColumnChannel).toString().endsWith("!video"))
        eventToFill.setIsVideoCall(true);

    event = eventToFill;
}

void QueryResult::parseHeaders(const QString &result,
                               QHash<QString, QString> &headers)
{
    /* Header format:
     * key1 1D value1 1F key2 1D value2 1F ...
     */

    QStringList headerList = result.split("\x1f", QString::SkipEmptyParts);
    foreach (QString header, headerList) {
        QStringList keyValue = header.split("\x1d");
        if (keyValue.isEmpty() || keyValue[0].isEmpty()) continue;
        headers.insert(keyValue[0], keyValue[1]);
    }
}

void QueryResult::parseContacts(const QString &result, const QString &localUid,
                                QList<Event::Contact> &contacts, ContactMap *contactMap)
{
    /* Tracker does not contain any contact info - we need to retrieve it separately.
     * The 'result' contains only remoteUids, to be resolved into contacts.
     */
    QStringList remoteUidList = result.split('\x1c', QString::SkipEmptyParts);
    contacts.reserve(contacts.count() + remoteUidList.count());

    QStringList unresolvedUidList;
    if (contactMap) {
        foreach (const QString &remoteUid, remoteUidList) {
            ContactMap::iterator it = contactMap->find(qMakePair(localUid, remoteUid));
            if (it != contactMap->end()) {
                // We already have this contact in the map
                const QList<Event::Contact> &cached(it.value());
                foreach (const Event::Contact &contact, cached) {
                    if (!contacts.contains(contact))
                        contacts.append(contact);
                }
                continue;
            }

            // We need to look this contact up
            unresolvedUidList.append(remoteUid);
        }
    } else {
        unresolvedUidList = remoteUidList;
    }

    if (!unresolvedUidList.isEmpty()) {
        // Find all contacts macthing each unresolved UID
        QHash<QString, QList<QContact> > matchingContacts(findMatchingContacts(localUid, unresolvedUidList));

        foreach (const QString &remoteUid, unresolvedUidList) {
            QList<Event::Contact> retrieved;

            const QList<QContact> &matched = matchingContacts[remoteUid];
            foreach (const QContact &match, matched) {
                Event::Contact contact;
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
                contact.first = match.id().toString().toInt();
#else
                contact.first = match.localId();
#endif

                QString firstName, lastName, contactNickname, imNickname;

                foreach (const QContactName &name, match.details<QContactName>()) {
                    if (!name.isEmpty()) {
                        firstName = name.firstName();
                        lastName = name.lastName();
                        break;
                    }
                }
                foreach (const QContactNickname &nickname, match.details<QContactNickname>()) {
                    if (!nickname.isEmpty()) {
                        contactNickname = nickname.nickname();
                        break;
                    }
                }
                foreach (const QContactPresence &presence, match.details<QContactPresence>()) {
                    if (!presence.isEmpty() && !presence.nickname().isEmpty()) {
                        imNickname = presence.nickname();
                        break;
                    }
                }

                contact.second = buildContactName(firstName, lastName, contactNickname, imNickname);

                if (!contacts.contains(contact))
                    contacts.append(contact);

                retrieved.append(contact);
            }

            if (!retrieved.isEmpty() && contactMap) {
                // Cache this result for later
                contactMap->insert(qMakePair(localUid, remoteUid), retrieved);
            }
        }
    }
}

QString QueryResult::buildContactName(const QString &firstName,
                                      const QString &lastName,
                                      const QString &contactNickname,
                                      const QString &imNickname)
{
    QString name;

    QString realName;
    if (!firstName.isEmpty() || !lastName.isEmpty()) {
        QString lname;
        if (ContactListener::instance()->isLastNameFirst())  {
            realName = lastName;
            lname = firstName;
        } else {
            realName = firstName;
            lname = lastName;
        }

        if (!lname.isEmpty()) {
            if (!realName.isEmpty())
                realName.append(' ');
            realName.append(lname);
        }
    }

    if (ContactListener::instance()->preferNickname()) {
        if (!contactNickname.isEmpty())
            name = contactNickname;
        else if (!realName.isEmpty())
            name = realName;
        else
            name = imNickname;
    } else {
        if (!realName.isEmpty())
            name = realName;
        else if (!imNickname.isEmpty())
            name = imNickname;
        else
            name = contactNickname;
    }

    return name;
}
