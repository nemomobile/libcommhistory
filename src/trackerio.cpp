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

#include <QSparqlConnectionOptions>
#include <QSparqlConnection>
#include <QSparqlResult>
#include <QSparqlResultRow>
#include <QSparqlError>

#include "commonutils.h"
#include "event.h"
#include "group.h"
#include "messagepart.h"
#include "mmscontentdeleter.h"
#include "updatequery.h"
#include "queryresult.h"
#include "committingtransaction.h"
#include "committingtransaction_p.h"
#include "eventsquery.h"
#include "preparedqueries.h"

#include "trackerio_p.h"
#include "trackerio.h"

using namespace CommHistory;

#define LAT(STR) QLatin1String(STR)

#define QSPARQL_DRIVER QLatin1String("QTRACKER_DIRECT")
#define QSPARQL_DATA_READY_INTERVAL 25

#define NMO_ "http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#"

Q_GLOBAL_STATIC(TrackerIO, trackerIO)

TrackerIOPrivate::TrackerIOPrivate(TrackerIO *parent)
    : q(parent),
    m_pTransaction(0),
    m_MmsContentDeleter(0),
    m_bgThread(0)
{
}

TrackerIOPrivate::~TrackerIOPrivate()
{
    foreach(CommittingTransaction* t, m_pendingTransactions)
        delete t;

    if (m_MmsContentDeleter) {
        m_MmsContentDeleter->deleteLater();
        m_MmsContentDeleter = 0;
    }
}

TrackerIO::TrackerIO()
    : d(new TrackerIOPrivate(this))
{
}

TrackerIO::~TrackerIO()
{
    delete d;
}

TrackerIO* TrackerIO::instance()
{
    return trackerIO();
}

int TrackerIO::nextEventId()
{
    return d->m_IdSource.nextEventId();
}

int TrackerIOPrivate::nextGroupId()
{
    return m_IdSource.nextGroupId();
}

QString TrackerIOPrivate::makeCallGroupURI(const CommHistory::Event &event)
{
    if (event.localUid().isEmpty() || event.remoteUid().isEmpty())
        return QString();

    QString callGroupRemoteId;
    QString number = normalizePhoneNumber(event.remoteUid());
    if (number.isEmpty()) {
        callGroupRemoteId = event.remoteUid();
    } else {
        // keep dial string in group uris for separate history entries
        callGroupRemoteId = makeShortNumber(event.remoteUid(), NormalizeFlagKeepDialString);
    }

    return QString(LAT("callgroup:%1!%2")).arg(event.localUid()).arg(callGroupRemoteId);
}

QString TrackerIOPrivate::prepareMessagePartQuery(const QString &messageUri)
{
    // NOTE: check MessagePartColumns in queryresult.h if you change this!
    QString query(LAT(
            "SELECT ?message "
              "?part "
              "?contentId "
              "nie:plainTextContent(?part) "
              "nie:mimeType(?part) "
              "nie:characterSet(?part) "
              "nie:contentSize(?part) "
              "nfo:fileName(?part) "
            "WHERE { "
              "?message  nmo:mmsHasContent [nie:hasPart ?part] . "
              "?part nmo:contentId ?contentId "
            "FILTER(?message = <%1>)} "
            "ORDER BY ?contentId"));

    return query.arg(messageUri);
}

QString TrackerIOPrivate::prepareGroupQuery(const QString &localUid,
                                             const QString &remoteUid,
                                             int groupId)
{
    QString queryFormat(GROUP_QUERY);
    QStringList constraints;
    if (!remoteUid.isEmpty()) {
        QString number = normalizePhoneNumber(remoteUid);
        if (number.isEmpty()) {
            constraints << QString(LAT("?channel nmo:hasParticipant [nco:hasIMAddress [nco:imID \"%1\"]] ."))
                          .arg(remoteUid);
        } else {
            constraints << QString(LAT("?channel nmo:hasParticipant [nco:hasPhoneNumber [maemo:localPhoneNumber \"%1\"]] ."))
                          .arg(number.right(CommHistory::phoneNumberMatchLength()));
        }
    }
    if (!localUid.isEmpty()) {
        constraints << QString(LAT("FILTER(nie:subject(?channel) = \"%1\") ")).arg(localUid);
    }
    if (groupId != -1)
        constraints << QString(LAT("FILTER(?channel = <%1>) ")).arg(Group::idToUrl(groupId).toString());

    return queryFormat.arg(constraints.join(LAT(" ")));
}

QString TrackerIOPrivate::prepareGroupedCallQuery()
{
    QString queryFormat(GROUPED_CALL_QUERY);
    return queryFormat;
}

QUrl TrackerIOPrivate::uriForIMAddress(const QString &account, const QString &remoteUid)
{
    return QUrl(QString(LAT("telepathy:")) + account + QLatin1Char('!') + remoteUid);
}

QString TrackerIOPrivate::findLocalContact(UpdateQuery &query,
                                           const QString &accountPath)
{
    Q_UNUSED(query);

    QString contact;
    QUrl uri = QString(LAT("telepathy:%1")).arg(accountPath);
    if (m_contactCache.contains(uri)) {
        contact = m_contactCache[uri];
    } else {
        contact = QString(LAT("[rdf:type nco:Contact; nco:hasIMAddress <%2>]"))
                  .arg(uri.toString());

        m_contactCache.insert(uri, contact);
    }

    return contact;
}

void TrackerIOPrivate::addIMContact(UpdateQuery &query,
                                    const QUrl &subject,
                                    const char *predicate,
                                    const QString &accountPath,
                                    const QString &imID)
{
    QString contact;
    QUrl imAddressURI = uriForIMAddress(accountPath, imID);

    if (m_contactCache.contains(imAddressURI)) {
        contact = m_contactCache[imAddressURI];
    } else {
        query.insertionRaw(imAddressURI,
                           "rdf:type",
                           LAT("nco:IMAddress"));
        query.insertion(imAddressURI,
                        "nco:imID",
                        imID);

        contact = QString(LAT("[rdf:type nco:Contact; nco:hasIMAddress <%2>]"))
            .arg(imAddressURI.toString());
        m_contactCache.insert(imAddressURI, contact);
    }

    query.insertionRaw(subject, predicate, contact);
}

void TrackerIOPrivate::addPhoneContact(UpdateQuery &query,
                                       const QUrl &subject,
                                       const char *predicate,
                                       const QString &phoneNumber,
                                       PhoneNumberNormalizeFlags normalizeFlags)
{
    Q_UNUSED(normalizeFlags);

    if (!m_contactCache.contains(phoneNumber)) {
        QString shortNumber = makeShortNumber(phoneNumber, normalizeFlags);
        QString phoneNumberInsert =
            QString(LAT("INSERT { _:_ a nco:PhoneNumber ; "
                        "nco:phoneNumber \"%1\" ; "
                        "maemo:localPhoneNumber \"%2\" . } "
                        "WHERE { "
                        "OPTIONAL { ?number nco:phoneNumber \"%1\" } "
                        "FILTER(!BOUND(?number)) "
                        "}"))
            .arg(phoneNumber)
            .arg(shortNumber);

        query.appendInsertion(phoneNumberInsert);
    }

    QString contactInsert =
        QString(LAT("INSERT { <%1> %2 [ a nco:Contact ; nco:hasPhoneNumber ?number ] } "
                    "WHERE { ?number nco:phoneNumber \"%3\" }"))
        .arg(subject.toString())
        .arg(predicate)
        .arg(phoneNumber);

    query.appendInsertion(contactInsert);
}

void TrackerIOPrivate::addRemoteContact(UpdateQuery &query,
                                        const QUrl &subject,
                                        const char *predicate,
                                        const QString &localUid,
                                        const QString &remoteUid,
                                        PhoneNumberNormalizeFlags normalizeFlags)
{
    QString phoneNumber = normalizePhoneNumber(remoteUid, normalizeFlags);
    if (phoneNumber.isEmpty()) {
        return addIMContact(query, subject, predicate, localUid, remoteUid);
    } else {
        return addPhoneContact(query, subject, predicate, remoteUid, normalizeFlags);
    }
}

void TrackerIOPrivate::TrackerIOPrivate::writeCommonProperties(UpdateQuery &query,
                                             Event &event,
                                             bool modifyMode)
{
    if (!modifyMode) { // ensure fields only when adding new events
        // make sure isDeleted and isDraft get set, we need them for queries
        if (event.type() != Event::CallEvent) {
            if (!event.validProperties().contains(Event::IsDeleted))
                event.setDeleted(false);
            if (!event.validProperties().contains(Event::IsDraft))
                event.setIsDraft(false);
        }

        // nmo:sentDate is used for indexing, make sure it's valid
        if (!event.validProperties().contains(Event::StartTime))
            event.setStartTime(QDateTime::fromTime_t(0));

        // also ensure valid nmo:isRead, filtering fails with libqttracker
        // (see bug #174248)
        if (!event.validProperties().contains(Event::IsRead))
            event.setIsRead(false);
    }

    Event::PropertySet propertySet = modifyMode ? event.modifiedProperties() : event.validProperties();
    foreach (Event::Property property, propertySet) {
        switch (property) {
        case Event::StartTime:
            if (event.startTime().isValid())
                query.insertion(event.url(),
                                "nmo:sentDate",
                                event.startTime(),
                                modifyMode);
            break;
        case Event::EndTime:
            if (event.endTime().isValid())
                query.insertion(event.url(),
                                "nmo:receivedDate",
                                event.endTime(),
                                modifyMode);
            break;
        case Event::Direction:
            if (event.direction() != Event::UnknownDirection) {
                bool isSent = event.direction() == Event::Outbound;
                query.insertion(event.url(),
                                "nmo:isSent",
                                isSent,
                                modifyMode);
            }
            break;
        case Event::IsDraft:
            query.insertion(event.url(),
                            "nmo:isDraft",
                            event.isDraft(),
                            modifyMode);
            break;
        case Event::IsRead:
            query.insertion(event.url(),
                            "nmo:isRead",
                            event.isRead(),
                            modifyMode);
            break;
        case Event::Status: {
            QUrl status;
            if (event.status() == Event::SentStatus) {
                status = LAT(NMO_ "delivery-status-sent");
            } else if (event.status() == Event::DeliveredStatus) {
                status = LAT(NMO_ "delivery-status-delivered");
            } else if (event.status() == Event::TemporarilyFailedStatus) {
                status = LAT(NMO_ "delivery-status-temporarily-failed");
            } else if (event.status() == Event::PermanentlyFailedStatus) {
                status = LAT(NMO_ "delivery-status-permanently-failed");
            }
            if (!status.isEmpty())
                query.insertion(event.url(),
                                "nmo:deliveryStatus",
                                status,
                                modifyMode);
            else if (modifyMode)
                query.deletion(event.url(),
                               "nmo:deliveryStatus");
            break;
        }
        case Event::ReadStatus: {
            QUrl readStatus;
            if (event.readStatus() == Event::ReadStatusRead) {
                readStatus = LAT(NMO_ "read-status-read");
            } else if (event.readStatus() == Event::ReadStatusDeleted) {
                readStatus = LAT(NMO_ "read-status-deleted");
            }
            if (!readStatus.isEmpty())
                query.insertion(event.url(),
                                "nmo:reportReadStatus",
                                readStatus,
                                modifyMode);
            break;
        }
        case Event::BytesReceived:
            query.insertion(event.url(),
                            "nie:contentSize",
                            event.bytesReceived(),
                            modifyMode);
            break;
        case Event::Subject:
            query.insertion(event.url(),
                            "nmo:messageSubject",
                            event.subject(),
                            modifyMode);
            break;
        case Event::FreeText:
            query.insertion(event.url(),
                            "nie:plainTextContent",
                            event.freeText(),
                            modifyMode);
            break;
        case Event::MessageToken:
            query.insertion(event.url(),
                            "nmo:messageId",
                            event.messageToken(),
                            modifyMode);
            break;
        case Event::MmsId:
            query.insertion(event.url(),
                            "nmo:mmsId",
                            event.mmsId(),
                            modifyMode);
            break;
        case Event::CharacterSet:
            query.insertion(event.url(),
                            "nmo:characterSet",
                            event.characterSet(),
                            modifyMode);
            break;
        case Event::Language:
            query.insertion(event.url(),
                            "nmo:language",
                            event.language(),
                            modifyMode);
            break;
        case Event::IsDeleted:
            query.insertion(event.url(),
                            "nmo:isDeleted",
                            event.isDeleted(),
                            modifyMode);
            break;
        case Event::ReportDelivery:
            query.insertion(event.url(),
                            "nmo:reportDelivery",
                            event.reportDelivery(),
                            modifyMode);
        case Event::ReportRead:
            query.insertion(event.url(),
                            "nmo:sentWithReportRead",
                            event.reportRead(),
                            modifyMode);
            break;
        case Event::ReportReadRequested:
            query.insertion(event.url(),
                            "nmo:mustAnswerReportRead",
                            event.reportReadRequested(),
                            modifyMode);
            break;
        default:; // do nothing
        }
    }

    // treat unknown status as sending for outbound SMS
    if ((event.status() == Event::UnknownStatus)
        && (event.type() == Event::SMSEvent || event.type() == Event::MMSEvent)
        && !event.isDraft() && event.direction() == Event::Outbound) {
        event.setStatus(Event::SendingStatus);
    }
}

void TrackerIOPrivate::writeSMSProperties(UpdateQuery &query,
                                          Event &event,
                                          bool modifyMode)
{
    Event::PropertySet propertySet = modifyMode ? event.modifiedProperties() : event.validProperties();
    foreach (Event::Property property, propertySet) {
        switch (property) {
        case Event::ParentId:
            query.insertion(event.url(),
                            "nmo:phoneMessageId",
                            event.parentId(),
                            modifyMode);
            break;
        case Event::Encoding:
            query.insertion(event.url(),
                            "nmo:encoding",
                            event.encoding(),
                            modifyMode);
            break;
        case Event::FromVCardFileName:
            if (!event.validProperties().contains(Event::FromVCardLabel)) {
                qWarning() << Q_FUNC_INFO << "VCardFileName without valid VCard label";
                continue;
            }

            // if there is no filename set for the vcard, then we don't save anything
            if (!event.fromVCardFileName().isEmpty()) {
                if (modifyMode)
                    query.resourceDeletion(event.url(),
                                           "nmo:fromVCard");

                QUrl vcard(LAT("_:vcard"));
                query.insertion(event.url(),
                                "nmo:fromVCard",
                                vcard);
                query.insertionRaw(vcard,
                                   "rdf:type",
                                   "nfo:FileDataObject");
                query.insertion(vcard,
                                "nfo:fileName",
                                event.fromVCardFileName());
                query.insertion(vcard,
                                "rdfs:label",
                                event.fromVCardLabel());
            }
            break;
        case Event::ValidityPeriod:
            query.insertion(event.url(),
                            "nmo:validityPeriod",
                            event.validityPeriod(),
                            modifyMode);
            break;
        default:; // do nothing
        }
    }
}

void TrackerIOPrivate::writeMMSProperties(UpdateQuery &query,
                                          Event &event,
                                          bool modifyMode)
{
    Event::PropertySet propertySet = modifyMode ? event.modifiedProperties() : event.validProperties();
    foreach (Event::Property property, propertySet) {
        switch (property) {
        case Event::MessageParts:
            if (modifyMode) {
                query.deletion(QString(LAT(
                        "DELETE  {<%1> nmo:hasAttachment ?_part . "
                        "?_part rdf:type rdfs:Resource ."
                        "} WHERE {<%1> nmo:mmsHasContent ?_content . "
                        "?_content nie:hasPart ?_part }"))
                               .arg(event.url().toString()));
                query.resourceDeletion(event.url(),
                                       "nmo:mmsHasContent");
            }
            addMessageParts(query, event);
            break;
        case Event::To:
            if (modifyMode)
                query.resourceDeletion(event.url(),
                                       "nmo:messageHeader");

            // Store To list in message header x-mms-to
            if (!event.toList().isEmpty()) {
                QUrl header(QString(LAT("_:header")));
                query.insertionRaw(header,
                                   "rdf:type",
                                   LAT("nmo:MessageHeader"));
                query.insertion(header,
                                "nmo:headerName",
                                LAT("x-mms-to"));
                query.insertion(header,
                                "nmo:headerValue",
                                event.toList().join("\x1e"));
                query.insertion(event.url(),
                                "nmo:messageHeader",
                                header);
            }
            break;
        case Event::Cc:
            if (modifyMode)
                query.deletion(event.url(),
                               "nmo:cc");

            foreach (QString contactString, event.ccList()) {
                addRemoteContact(query, event.url(), "nmo:cc", event.localUid(), contactString);
            }
            break;
        case Event::Bcc:
            if (modifyMode)
                query.deletion(event.url(),
                               "nmo:bcc");

            foreach (QString contactString, event.bccList()) {
                addRemoteContact(query, event.url(), "nmo:bcc", event.localUid(), contactString);
            }
            break;
        case Event::ContentLocation:
            query.insertion(event.url(),
                            "nie:generator",
                            event.contentLocation(),
                            modifyMode);
            break;
        default:; //do nothing
        }
    }
}

void TrackerIOPrivate::writeCallProperties(UpdateQuery &query, Event &event, bool modifyMode)
{
    query.insertion(event.url(),
                    "nmo:isAnswered",
                    !event.isMissedCall(),
                    modifyMode);
    query.insertion(event.url(),
                    "nmo:isEmergency",
                    event.isEmergencyCall(),
                    modifyMode);
    query.insertion(event.url(),
                    "nmo:duration",
                    (int)(event.endTime().toTime_t() - event.startTime().toTime_t()),
                    modifyMode);
}

void TrackerIOPrivate::addMessageParts(UpdateQuery &query, Event &event)
{
    QUrl content(QString(LAT("_:content")));
    QUrl eventSubject(event.url());

    query.insertionRaw(content,
                       "rdf:type",
                       LAT("nmo:Multipart"));
    query.insertion(eventSubject,
                    "nmo:mmsHasContent",
                    content);

    int i = 0;
    foreach (const MessagePart &messagePart, event.messageParts()) {
        QUrl part(QString(LAT("_:part%1")).arg(i++));
        query.insertionRaw(part,
                           "rdf:type",
                           LAT("nmo:Attachment"));

        // TODO: how exactly should we handle these?
        if (messagePart.contentType().startsWith(LAT("image/"))) {
            query.insertionRaw(part,
                               "rdf:type",
                               LAT("nfo:Image"));
        } else if (messagePart.contentType().startsWith(LAT("audio/"))) {
            query.insertionRaw(part,
                               "rdf:type",
                               LAT("nfo:Audio"));
        } else if (messagePart.contentType().startsWith(LAT("video/"))) {
            query.insertionRaw(part,
                               "rdf:type",
                               LAT("nfo:Video"));
        } else {
            query.insertionRaw(part,
                               "rdf:type",
                               LAT("nfo:PlainTextDocument"));
        }

        // set nmo:MimePart properties
        query.insertion(part,
                        "nmo:contentId",
                        messagePart.contentId());
        query.insertion(part,
                        "nie:contentSize",
                        messagePart.contentSize());

        query.insertion(part,
                        "nfo:fileName",
                        messagePart.contentLocation());

        // set nie:InformationElement properties
        query.insertion(part,
                        "nie:plainTextContent",
                        messagePart.plainTextContent());
        query.insertion(part,
                        "nie:mimeType",
                        messagePart.contentType());
        query.insertion(part,
                        "nie:characterSet",
                        messagePart.characterSet());
        query.insertion(content,
                        "nie:hasPart",
                        part);

        if (messagePart.contentType() != LAT("text/plain") &&
            messagePart.contentType() != LAT("application/smil"))
        {
            qDebug() << "[MMS-ATTACH] Adding attachment" << messagePart.contentLocation() << messagePart.contentType() << "to message" << event.url();
            query.insertion(eventSubject,
                            "nmo:hasAttachment",
                            part);
        }
    }
}

void TrackerIOPrivate::addIMEvent(UpdateQuery &query, Event &event)
{
    event.setId(m_IdSource.nextEventId());
    QUrl eventSubject(event.url());

    query.insertionRaw(eventSubject,
                       "rdf:type",
                       LAT("nmo:IMMessage"));

    QString localContact = findLocalContact(query, event.localUid());

    if (event.direction() == Event::Outbound) {
        query.insertionRaw(eventSubject,
                           "nmo:from",
                           localContact);
        addIMContact(query, eventSubject, "nmo:to", event.localUid(), event.remoteUid());
    } else {
        query.insertionRaw(eventSubject,
                           "nmo:to",
                           localContact);
        addIMContact(query, eventSubject, "nmo:from", event.localUid(), event.remoteUid());
    }
    writeCommonProperties(query, event, false);
}

void TrackerIOPrivate::addSMSEvent(UpdateQuery &query, Event &event)
{
    event.setId(m_IdSource.nextEventId());
    QUrl eventSubject(event.url());

    if (event.type() == Event::MMSEvent) {
        query.insertionRaw(eventSubject,
                           "rdf:type",
                           LAT("nmo:MMSMessage"));
    } else if (event.type() == Event::SMSEvent) {
        query.insertionRaw(eventSubject,
                           "rdf:type",
                           LAT("nmo:SMSMessage"));
    }

    //TODO: add check that group exist as part of the query
    writeCommonProperties(query, event, false);
    writeSMSProperties(query, event, false);
    if (event.type() == Event::MMSEvent)
        writeMMSProperties(query, event, false);

    QString localContact = findLocalContact(query, event.localUid());

    if (event.direction() == Event::Outbound) {
        query.insertionRaw(eventSubject,
                           "nmo:from",
                           localContact);
        addRemoteContact(query, eventSubject, "nmo:to", event.localUid(), event.remoteUid(),
                         NormalizeFlagKeepDialString);
    } else {
        query.insertionRaw(eventSubject,
                           "nmo:to",
                           localContact);
        addRemoteContact(query, eventSubject, "nmo:from", event.localUid(), event.remoteUid(),
                         NormalizeFlagKeepDialString);
    }
}

void TrackerIOPrivate::addCallEvent(UpdateQuery &query, Event &event)
{
    event.setId(m_IdSource.nextEventId());
    QUrl eventSubject(event.url());

    query.insertionRaw(eventSubject,
                       "rdf:type",
                       LAT("nmo:Call"));

    QString localContact = findLocalContact(query, event.localUid());

    if (event.direction() == Event::Outbound) {
        query.insertionRaw(eventSubject,
                           "nmo:from",
                           localContact);
        addRemoteContact(query, eventSubject, "nmo:to", event.localUid(), event.remoteUid(),
                         NormalizeFlagKeepDialString);
    } else {
        query.insertionRaw(eventSubject,
                           "nmo:to",
                           localContact);
        addRemoteContact(query, eventSubject, "nmo:from", event.localUid(), event.remoteUid(),
                         NormalizeFlagKeepDialString);
    }

    writeCommonProperties(query, event, false);

    writeCallProperties(query, event, false);

    QUrl channelUri = makeCallGroupURI(event);
    QString localUri = QString(LAT("telepathy:%1")).arg(event.localUid());
    QString eventDate(event.endTime().toUTC().toString(Qt::ISODate));

    query.insertionSilent(
        QString(LAT("GRAPH <%1> { <%2> a nmo:CommunicationChannel }"))
        .arg(COMMHISTORY_GRAPH_CALL_CHANNEL)
        .arg(channelUri.toString()));

    // TODO: would it be cleaner to have all channel-related triples
    // inside the call group graph?
    query.insertion(channelUri, "nmo:lastMessageDate", event.startTime(), true);
    query.insertion(channelUri, "nie:contentLastModified", event.startTime(), true);
    query.deletion(channelUri, "nmo:hasParticipant");
    addRemoteContact(query, channelUri, "nmo:hasParticipant", event.localUid(), event.remoteUid(),
                     NormalizeFlagKeepDialString);

    if (!event.isMissedCall()) {
        query.insertion(channelUri, "nmo:lastSuccessfulMessageDate", event.startTime(), true);
    } else {
        // ensure existence of nmo:lastSuccessfulMessageDate
        query.insertionSilent(
            QString(LAT("<%1> nmo:lastSuccessfulMessageDate "
                        "\"1970-01-01T00:00:00Z\"^^xsd:dateTime ."))
            .arg(channelUri.toString()));
    }

    query.insertion(eventSubject, "nmo:communicationChannel", channelUri);
}

void TrackerIOPrivate::setChannel(UpdateQuery &query, Event &event, int channelId, bool modify)
{
    QUrl channelUrl = Group::idToUrl(channelId);

    query.insertion(channelUrl,
                    "nmo:lastMessageDate",
                    event.endTime(),
                    true);
    query.insertion(channelUrl,
                    "nie:contentLastModified",
                    event.endTime(),
                    true);
    query.insertion(event.url(),
                    "nmo:communicationChannel",
                    channelUrl,
                    modify);
}

void TrackerIOPrivate::doUpdateGroupTimestamps(CommittingTransaction *transaction,
                                               QSparqlResult *result,
                                               QVariant arg)
{
    Q_UNUSED(transaction);

    QString groupUri = arg.toString();
    qDebug() << Q_FUNC_INFO << groupUri;

    QDateTime lastMessageDate;
    QDateTime lastSuccessfulMessageDate;

    if (result && result->first()) {
        lastMessageDate = result->value(0).toDateTime();
        lastSuccessfulMessageDate = result->value(1).toDateTime();
    }

    if (lastMessageDate.isValid() || lastSuccessfulMessageDate.isValid()) {
        UpdateQuery update;
        if (lastMessageDate.isValid()) {
            update.insertion(groupUri,
                             "nmo:lastMessageDate",
                             lastMessageDate,
                             true);
        }

        if (lastSuccessfulMessageDate.isValid()) {
            update.insertion(groupUri,
                             "nmo:lastSuccessfulMessageDate",
                             lastSuccessfulMessageDate,
                             true);
        }

        handleQuery(QSparqlQuery(update.query(), QSparqlQuery::InsertStatement));
    }
}

void TrackerIOPrivate::updateGroupTimestamps(CommittingTransaction *transaction,
                                             QSparqlResult *result,
                                             QVariant arg)
{
    Q_UNUSED(transaction);
    Q_UNUSED(result);

    Event event = qVariantValue<CommHistory::Event>(arg);
    qDebug() << Q_FUNC_INFO << event.type() << event.groupId();

    if (event.type() != Event::CallEvent && event.groupId() == -1) return;

    QDateTime lastMessageDate;
    QDateTime lastSuccessfulMessageDate;
    QString groupUri;
    QString timeProperty;
    if (event.type() == Event::CallEvent) {
        groupUri = makeCallGroupURI(event);
        timeProperty = QLatin1String("nmo:sentDate");
    } else {
        groupUri = Group::idToUrl(event.groupId()).toString();
        timeProperty = QLatin1String("nmo:receivedDate");
    }

    if (groupUri.isEmpty()) return;

    // get last message time
    QString timestampQuery =
        QString(LAT("SELECT "
                    "(SELECT ?lastDate { ?lastMessage nmo:communicationChannel ?channel ; "
                    "   %1 ?lastDate . } ORDER BY DESC(?lastDate)) "
                    "(SELECT ?lastSuccessfulDate { "
                    " ?lastMessage nmo:communicationChannel ?channel ; "
                    " %1 ?lastSuccessfulDate . "
                    " FILTER(nmo:isSent(?lastMessage) = true || "
                    "   nmo:isAnswered(?lastMessage) = true) "
                    "} ORDER BY DESC(?lastSuccessfulDate)) "
                    "WHERE { "
                    " ?channel a nmo:CommunicationChannel . "
                    " FILTER(?channel = ?:channel) }"))
        .arg(timeProperty);

    QSparqlQuery query(timestampQuery);
    query.bindValue(LAT("channel"), QUrl(groupUri));

    handleQuery(QSparqlQuery(query), this,
                "doUpdateGroupTimestamps",
                QVariant(groupUri));
}

bool TrackerIO::addEvent(Event &event)
{
    UpdateQuery query;

    // TODO: maybe check uri prefix for localUid?
    if (event.type() == Event::IMEvent
        || event.type() == Event::SMSEvent
        || event.type() == Event::MMSEvent) {
        if (event.type() == Event::IMEvent) {
            d->addIMEvent(query, event);
        } else {
            if (event.parentId() < 0) {
                d->calculateParentId(event);
            }
            d->addSMSEvent(query, event);

            //setting the time at which the folder was last updated
            d->setFolderLastModifiedTime(query, event.parentId(), QDateTime::currentDateTime());
        }

        // specify not-inherited classes only when adding events, not during modifications
        // to have tracker errors when modifying non-existent events
        query.insertionRaw(event.url(),
                           "rdf:type",
                           LAT("nie:DataObject"));

        if (!event.isDraft()) {
            // TODO: add missing participants
            //LiveNodes participants = channel->getHasParticipants();
            d->setChannel(query, event, event.groupId());
        }
    } else if (event.type() == Event::CallEvent) {
        d->addCallEvent(query, event);
    } else if (event.type() != Event::StatusMessageEvent) {
        qWarning() << "event type not implemented";
        return false;
    }


    event.setLastModified(QDateTime::currentDateTime());
    query.insertion(event.url(),
                    "nie:contentLastModified",
                    event.lastModified());

    return d->handleQuery(QSparqlQuery(query.query(),
                                       QSparqlQuery::InsertStatement));
}

bool TrackerIO::addGroup(Group &group)
{
    UpdateQuery query;

    if (group.localUid().isEmpty()) {
        qWarning() << "Local uid required for group";
        return false;
    }

    group.setId(d->nextGroupId());

    qDebug() << __FUNCTION__ << group.url() << group.localUid() << group.remoteUids();

    QString channelSubject = group.url().toString();

    query.insertion(QString(LAT("GRAPH <%1> { <%2> a nmo:CommunicationChannel }"))
                    .arg(COMMHISTORY_GRAPH_MESSAGE_CHANNEL)
                    .arg(channelSubject));

    // TODO: ontology
    query.insertion(channelSubject,
                    "nie:subject",
                    group.localUid());

    query.insertion(channelSubject,
                    "nie:identifier",
                    QString::number(group.chatType()));

    query.insertion(channelSubject,
                    "nie:title",
                    group.chatName());

    QString remoteUid = group.remoteUids().first();
    QString phoneNumber = normalizePhoneNumber(remoteUid);

    if (phoneNumber.isEmpty()) {
        d->addIMContact(query, channelSubject, "nmo:hasParticipant",
                        group.localUid(), remoteUid);
        query.insertion(channelSubject,
                        "nie:generator",
                        remoteUid);
    } else {
        d->addPhoneContact(query, channelSubject, "nmo:hasParticipant", remoteUid,
                           NormalizeFlagRemovePunctuation);
        query.insertion(channelSubject,
                        "nie:generator",
                        remoteUid);
    }

    query.insertion(channelSubject,
                    "nmo:lastMessageDate",
                    QDateTime::fromTime_t(0));

    group.setLastModified(QDateTime::currentDateTime());
    query.insertion(channelSubject,
                    "nie:contentLastModified",
                    group.lastModified());

    return d->handleQuery(QSparqlQuery(query.query(),
                                       QSparqlQuery::InsertStatement));
}

bool TrackerIOPrivate::querySingleEvent(EventsQuery &query, Event &event)
{
    QueryResult result;
    QScopedPointer<QSparqlResult> events(connection().exec(QSparqlQuery(query.query())));

    if (!runBlockedQuery(events.data())) // FIXIT
        return false;

    result.result = events.data();
    result.properties = query.eventProperties();

    if (!events->first()) {
        qWarning() << "Event not found";
        return false;
    }

    QSparqlResultRow row = events->current();

    result.fillEventFromModel(event);

    if (event.type() == Event::MMSEvent) {
        QString partQuery = TrackerIOPrivate::prepareMessagePartQuery(event.url().toString());

        QScopedPointer<QSparqlResult> parts(connection().exec(QSparqlQuery(partQuery)));

        if (runBlockedQuery(parts.data()) &&
            parts->size() > 0) {
            result.result = parts.data();

            parts->first();
            QSparqlResultRow partRow = parts->current();

            do {
                MessagePart part;
                result.fillMessagePartFromModel(part);
                event.addMessagePart(part);
            } while (parts->next());
        }
        event.resetModifiedProperties();
    }

    return true;
}

bool TrackerIO::getEvent(int id, Event &event)
{
    qDebug() << Q_FUNC_INFO << id;
    EventsQuery query(Event::allProperties());

    query.addPattern(QString(LAT("FILTER(%2 = <%1>)"))
                     .arg(Event::idToUrl(id).toString()))
                    .variable(Event::Id);

    return d->querySingleEvent(query, event);
}

bool TrackerIO::getEventByMessageToken(const QString& token, Event &event)
{
    EventsQuery query(Event::allProperties());

    query.addPattern(QString(LAT("%2 nmo:messageId \"%1\" ."))
                     .arg(token))
                    .variable(Event::Id);

    return d->querySingleEvent(query, event);
}

bool TrackerIO::getEventByMessageToken(const QString &token, int groupId, Event &event)
{
    EventsQuery query(Event::allProperties());

    query.addPattern(QString(LAT("%3 nmo:messageId \"%1\";"
                                           "nmo:communicationChannel <%2> ."))
                     .arg(token)
                     .arg(Group::idToUrl(groupId).toString()))
                    .variable(Event::Id);

    return d->querySingleEvent(query, event);
}

bool TrackerIO::getEventByMmsId(const QString& mmsId, int groupId, Event &event)
{
    EventsQuery query(Event::allProperties());

    query.addPattern(QString(LAT("%3 nmo:mmsId \"%1\";"
                                           "nmo:isSent \"true\";"
                                           "nmo:communicationChannel <%2> ."))
                     .arg(mmsId)
                     .arg(Group::idToUrl(groupId).toString()))
                    .variable(Event::Id);

    return d->querySingleEvent(query, event);
}

bool TrackerIO::getEventByUri(const QUrl &uri, Event &event)
{
    int eventId = Event::urlToId(uri.toString());
    return getEvent(eventId, event);
}

bool TrackerIO::modifyEvent(Event &event)
{
    UpdateQuery query;

    event.setLastModified(QDateTime::currentDateTime()); // always update modified times in case of modifyEvent
                                                         // irrespective whether client sets or not
                                                         // allow uid changes for drafts
    if (event.isDraft()
        && (event.validProperties().contains(Event::LocalUid)
            || event.validProperties().contains(Event::RemoteUid))) {
        // TODO: allow multiple remote uids
        QString localContact = d->findLocalContact(query, event.localUid());
        if (event.direction() == Event::Outbound) {
            query.insertionRaw(event.url(),
                               "nmo:from",
                               localContact);
            query.deletion(event.url(), "nmo:to");
            d->addRemoteContact(query, event.url(), "nmo:to",
                                event.localUid(), event.remoteUid(),
                                NormalizeFlagKeepDialString);
        } else {
            query.insertionRaw(event.url(),
                               "nmo:to",
                               localContact);
            d->addRemoteContact(query, event.url(), "nmo:from",
                                event.localUid(), event.remoteUid(),
                                NormalizeFlagKeepDialString);
        }
    }

    if (event.type() == Event::SMSEvent) {
        if (event.status() == Event::DeliveredStatus
            || event.status() == Event::SentStatus) {
            qDebug() << "Sms sent, updating parent id to SENT id";
            event.setParentId(SENT);
        }

        d->writeSMSProperties(query, event, true);
    }

    if (event.type() == Event::MMSEvent)
        d->writeMMSProperties(query, event, true);

    if (event.modifiedProperties().contains(Event::LastModified)
        && event.lastModified() > QDateTime::fromTime_t(0)) {
        query.insertion(event.url(),
                        "nie:contentLastModified",
                        event.lastModified(),
                        true);
    }

    if (event.type() == Event::CallEvent)
        d->writeCallProperties(query, event, true);

    d->writeCommonProperties(query, event, true);

    return d->handleQuery(QSparqlQuery(query.query(), QSparqlQuery::InsertStatement),
                          d, "updateGroupTimestamps",
                          QVariant::fromValue(event));
}

bool TrackerIO::modifyGroup(Group &group)
{
    UpdateQuery query;

    Group::PropertySet propertySet = group.modifiedProperties();
    foreach (Group::Property property, propertySet) {
        switch (property) {
            case Group::ChatName:
                query.insertion(group.url(),
                                "nie:title",
                                group.chatName(),
                                true);
                break;
            case Group::EndTime:
                if (group.endTime().isValid())
                    query.insertion(group.url(),
                                    "nmo:lastMessageDate",
                                    group.endTime(),
                                    true);
                break;
            case Group::LastModified:
                if ( group.lastModified() > QDateTime::fromTime_t(0) )
                    query.insertion(group.url(),
                                    "nie:contentLastModified",
                                    group.lastModified(),
                                    true);
            default:; // do nothing
        }
    }

    return d->handleQuery(QSparqlQuery(query.query(),
                                       QSparqlQuery::InsertStatement));
}

bool TrackerIO::moveEvent(Event &event, int groupId)
{
    UpdateQuery query;

    d->setChannel(query, event, groupId, true); // true means modify

    if (event.direction() == Event::Inbound) {
        query.deletion(event.url(), "nmo:from");
        d->addRemoteContact(query, event.url(), "nmo:from", event.localUid(), event.remoteUid(),
                            NormalizeFlagKeepDialString);
    }

    return d->handleQuery(QSparqlQuery(query.query(),
                                       QSparqlQuery::InsertStatement));
}

bool TrackerIO::deleteEvent(Event &event, QThread *backgroundThread)
{
    qDebug() << Q_FUNC_INFO << event.id() << event.localUid() << event.remoteUid() << backgroundThread;

    if (event.type() == Event::MMSEvent) {
        if (d->isLastMmsEvent(event.messageToken())) {
            d->getMmsDeleter(backgroundThread).deleteMessage(event.messageToken());
        }
        Q_ASSERT(d->m_pTransaction);
        connect(d->m_pTransaction,SIGNAL(finished()),d,SLOT(requestCountMmsEvents()),Qt::UniqueConnection);
    }

    QString query(LAT("DELETE {?:uri a rdfs:Resource}"));

    // delete empty call groups
    if (event.type() == Event::CallEvent) {
        query += LAT(
            "DELETE { ?chan a rdfs:Resource } WHERE { "
            "GRAPH ?:graph { "
            "?chan a nmo:CommunicationChannel . "
            "} "
            "OPTIONAL { "
            "?call a nmo:Call ; "
            "nmo:communicationChannel ?chan . "
            "} "
            "FILTER (!BOUND(?call)) "
            "}");
    }

    QSparqlQuery deleteQuery(query, QSparqlQuery::DeleteStatement);
    deleteQuery.bindValue(LAT("uri"), event.url());

    if (event.type() == Event::CallEvent)
        deleteQuery.bindValue(LAT("graph"), COMMHISTORY_GRAPH_CALL_CHANNEL);

    return d->handleQuery(deleteQuery, d,
                          "updateGroupTimestamps",
                          QVariant::fromValue(event));
}

bool TrackerIO::getGroup(int id, Group &group)
{
    Group groupToFill;

    QSparqlQuery query(TrackerIOPrivate::prepareGroupQuery(QString(), QString(), id));
    QueryResult result;

    QScopedPointer<QSparqlResult> groups(d->connection().exec(query));

    if (!d->runBlockedQuery(groups.data())) // FIXIT
        return false;

    if (groups->size() == 0) {
        qWarning() << "Group not found";
        return false;
    }

    groups->first();
    QSparqlResultRow row = groups->current();

    result.result = groups.data();
    result.fillGroupFromModel(groupToFill);
    group = groupToFill;

    return true;
}

QSparqlResult* TrackerIOPrivate::getMmsListForDeletingByGroup(int groupId)
{
    QSparqlQuery query(LAT(
            "SELECT ?message "
              "nmo:messageId(?message) "
              "(SELECT COUNT(?otherMessage) "
                "WHERE {"
                "?otherMessage rdf:type nmo:MMSMessage; nmo:isDeleted \"false\" "
                "FILTER(nmo:messageId(?otherMessage) = nmo:messageId(?message))})"
            "WHERE {"
              "?message rdf:type nmo:MMSMessage; nmo:communicationChannel ?:conversation}"));

    query.bindValue(LAT("conversation"), Group::idToUrl(groupId));

    return connection().exec(query);
}

bool TrackerIOPrivate::deleteMmsContentByGroup(int groupId)
{
    // delete mms messages content from fs
    QScopedPointer<QSparqlResult> result(getMmsListForDeletingByGroup(groupId));

    if (!runBlockedQuery(result.data())) // FIXIT
        return false;

    while (result->next()) {
        QSparqlResultRow row = result->current();

        if (row.isEmpty()) {
            qWarning() << Q_FUNC_INFO << "Empty row";
            continue;
        }

        QString messageToken(row.value(1).toString());
        qDebug() << Q_FUNC_INFO << messageToken;

        if (!messageToken.isEmpty()) {
            int refCount(row.value(2).toInt());

            MessageTokenRefCount::iterator it = m_messageTokenRefCount.find(messageToken);

            // Update cache
            if (it != m_messageTokenRefCount.end()) {
                // Message token is already in cache -> decrease refcount since  message is going to be removed
                --(it.value());
            } else {
                // Message token is not cache -> add into cache, decrease ref count by 1
                m_messageTokenRefCount[messageToken] = refCount - 1;
            }
        }
    }

    return true;
}

bool TrackerIO::deleteGroup(int groupId, bool deleteMessages, QThread *backgroundThread)
{
    Q_UNUSED(backgroundThread);
    qDebug() << __FUNCTION__ << groupId << deleteMessages << backgroundThread;

    // error return left for possible future implementation

    QUrl group = Group::idToUrl(groupId);
    UpdateQuery update;

    if (deleteMessages) {
        update.deletion(QString(LAT(
                "DELETE {?msg rdf:type rdfs:Resource}"
                "WHERE {?msg rdf:type nmo:Message; nmo:communicationChannel <%1>}"))
                        .arg(group.toString()));

        // delete mms attachments
        // FIXIT, make it async
        if (!d->deleteMmsContentByGroup(groupId))
            return false;

        Q_ASSERT(d->m_pTransaction);
        connect(d->m_pTransaction,SIGNAL(finished()),d,SLOT(requestCountMmsEvents()),Qt::UniqueConnection);
    }

    d->m_bgThread = backgroundThread;

    // delete conversation
    update.deletion(group,
                    "rdf:type",
                    LAT("rdfs:Resource"));

    return d->handleQuery(QSparqlQuery(update.query(),
                                       QSparqlQuery::DeleteStatement));
}

bool TrackerIO::totalEventsInGroup(int groupId, int &totalEvents)
{
    QSparqlQuery query(LAT(
            "SELECT COUNT(?message) "
            "WHERE {"
              "?message rdf:type nmo:Message; nmo:isDeleted \"false\";"
              "nmo:communicationChannel ?:conversation}"));

    query.bindValue(LAT("conversation"), Group::idToUrl(groupId));

    QScopedPointer<QSparqlResult> queryResult(d->connection().exec(query));

    if (!d->runBlockedQuery(queryResult.data())) // FIXIT
        return false;

    if (queryResult->first()) {
        QSparqlResultRow row = queryResult->current();
        if (!row.isEmpty()) {
            totalEvents = row.value(0).toInt();
            return true;
        }
    }

    return false;
}

bool TrackerIO::markAsReadGroup(int groupId)
{
    QSparqlQuery query(LAT(
            "DELETE {?msg nmo:isRead ?r; nie:contentLastModified ?d}"
            "WHERE {?msg rdf:type nmo:Message; nmo:communicationChannel ?:conversation;"
                   "nmo:isRead ?r; nie:contentLastModified ?d}"
            "INSERT {?msg nmo:isRead ?:read; nie:contentLastModified ?:date}"
            "WHERE {?msg rdf:type nmo:Message; nmo:communicationChannel ?:conversation}"),
                       QSparqlQuery::InsertStatement);

    query.bindValue(LAT("conversation"), Group::idToUrl(groupId));
    query.bindValue(LAT("read"), true);
    //Need to update the contentModifiedTime as well so that NOS gets update with the updated time
    query.bindValue(LAT("date"), QDateTime::currentDateTime());

    return d->handleQuery(query);
}

void TrackerIO::transaction(bool syncOnCommit)
{
    Q_ASSERT(d->m_pTransaction == 0);

    d->syncOnCommit = syncOnCommit; //TODO: support syncOnCommit, call tracker's Sync dbus method directly
    d->m_pTransaction = new CommittingTransaction(this);
    d->m_messageTokenRefCount.clear(); // make sure that nothing is removed if not requested
}

CommittingTransaction* TrackerIO::commit(bool isBlocking)
{
    Q_ASSERT(d->m_pTransaction);

    CommittingTransaction *returnTransaction = 0;

    if (isBlocking) {
        d->m_pTransaction->run(d->connection(), true);
        delete d->m_pTransaction;
    } else {
        if (!d->m_pTransaction->d->isEmpty()) {
            d->m_pendingTransactions.enqueue(d->m_pTransaction);
            d->runNextTransaction();
            returnTransaction = d->m_pTransaction;
        } else {
            qWarning() << Q_FUNC_INFO << "Empty transaction committing";
            delete d->m_pTransaction;
        }

    }

    d->m_contactCache.clear();
    d->checkAndDeletePendingMmsContent(d->m_bgThread);
    d->m_pTransaction = 0;

    return returnTransaction;
}

void TrackerIOPrivate::runNextTransaction()
{
    qDebug() << Q_FUNC_INFO;

    if (m_pendingTransactions.isEmpty())
        return;

    CommittingTransaction *t = m_pendingTransactions.head();

    Q_ASSERT(t);

    if (t->isFinished()) {
        delete t;
        m_pendingTransactions.dequeue();
        t = 0;

        if (!m_pendingTransactions.isEmpty())
            t = m_pendingTransactions.head();
    }

    if (t && !t->isRunning()) {
        if (!t->run(connection())) {
            qWarning() << Q_FUNC_INFO << "abort transaction" << t;
            emit t->finished();
            m_pendingTransactions.dequeue();
            delete t;
            t = 0;
        } else {
            connect(t,
                    SIGNAL(finished()),
                    this,
                    SLOT(runNextTransaction()));
        }
    }
}

void TrackerIO::rollback()
{
    d->m_contactCache.clear();
    d->m_messageTokenRefCount.clear(); // Clear cache to avoid deletion after rollback
    delete d->m_pTransaction;
    d->m_pTransaction = 0;
}

bool TrackerIO::deleteAllEvents(Event::EventType eventType)
{
    qDebug() << __FUNCTION__ << eventType;

    QString query("DELETE {?e a rdfs:Resource}"
                  "WHERE {?e rdf:type ?:eventType}");

    QUrl eventTypeUrl;

    switch(eventType) {
    case Event::IMEvent:
        eventTypeUrl = QUrl(LAT(NMO_ "IMMessage"));
        break;
    case Event::SMSEvent:
        eventTypeUrl = QUrl(LAT(NMO_ "SMSMessage"));
        break;
    case Event::CallEvent:
        eventTypeUrl = QUrl(LAT(NMO_ "Call"));
        query += QString(LAT("DELETE { ?chan a rdfs:Resource } WHERE { "
                             "GRAPH ?:graph { "
                             "?chan a nmo:CommunicationChannel . "
                             "} }"));
        break;
    case Event::MMSEvent:
        eventTypeUrl = QUrl(LAT(NMO_ "MMSMessage"));
        Q_ASSERT(d->m_pTransaction);
        connect(d->m_pTransaction,SIGNAL(finished()),d,SLOT(requestCountMmsEvents()),Qt::UniqueConnection);
        break;
    default:
        qWarning() << __FUNCTION__ << "Unsupported type" << eventType;
        return false;
    }

    QSparqlQuery deleteQuery(query, QSparqlQuery::DeleteStatement);
    deleteQuery.bindValue(LAT("eventType"), eventTypeUrl);
    if (eventType == Event::CallEvent)
        deleteQuery.bindValue(LAT("graph"), COMMHISTORY_GRAPH_CALL_CHANNEL);

    return d->handleQuery(deleteQuery);
}

void TrackerIOPrivate::calculateParentId(Event& event)
{
    if (event.isDraft()) {
        event.setParentId(DRAFT);
    } else if (event.direction() == Event::Inbound) {
        event.setParentId(INBOX);
    } else if  (event.direction() == Event::Outbound) {
        event.setParentId(SENT);
        if (event.status() == Event::SendingStatus) {
            event.setParentId(OUTBOX);
        }
    }
}

void TrackerIOPrivate::setFolderLastModifiedTime(UpdateQuery &query,
                                                 int parentId,
                                                 const QDateTime& lastModTime)
{
    QUrl folder;
    if (parentId == ::INBOX) {
        folder = LAT(NMO_ "predefined-phone-msg-folder-inbox");
    } else if (parentId == ::OUTBOX) {
        folder = LAT(NMO_ "predefined-phone-msg-folder-outbox");
    } else if (parentId == ::DRAFT) {
        folder = LAT(NMO_ "predefined-phone-msg-folder-draft");
    } else if (parentId == ::SENT) {
        folder = LAT(NMO_ "predefined-phone-msg-folder-sent");
    } else if (parentId == ::MYFOLDER) {
        folder = LAT(NMO_ "predefined-phone-msg-folder-myfolder");
    } else if (parentId > ::MYFOLDER) {
        // TODO: should this be nmo: prefixed?
        folder = QString(LAT("sms-folder-myfolder-0x%1")).arg(parentId, 0, 16);
    }

    if (!folder.isEmpty()) {
        query.insertion(folder,
                        "nie:contentLastModified",
                        lastModTime,
                        true);
    }
}

bool TrackerIO::markAsRead(const QList<int> &eventIds)
{
    UpdateQuery query;
    foreach (int id, eventIds) {
        query.insertion(Event::idToUrl(id),
                        "nmo:isRead",
                        true,
                        true);
    }

    return d->handleQuery(QSparqlQuery(query.query(),
                                       QSparqlQuery::InsertStatement));
}

MmsContentDeleter& TrackerIOPrivate::getMmsDeleter(QThread *backgroundThread)
{
    if (m_MmsContentDeleter && backgroundThread) {
        // check that we don't need to move deleter to new thread
        if (m_MmsContentDeleter->thread() != backgroundThread) {
            m_MmsContentDeleter->deleteLater();
            m_MmsContentDeleter = 0;
        }
    }

    if (!m_MmsContentDeleter) {
        m_MmsContentDeleter = new MmsContentDeleter();

        if (backgroundThread)
            m_MmsContentDeleter->moveToThread(backgroundThread);
    }

    Q_ASSERT(m_MmsContentDeleter != 0);
    return *m_MmsContentDeleter;
}

bool TrackerIOPrivate::isLastMmsEvent(const QString &messageToken)
{
    qDebug() << Q_FUNC_INFO << messageToken;
    int total = -1;

    QSparqlQuery query(LAT(
            "SELECT COUNT(?message) "
            "WHERE {?message rdf:type nmo:MMSMessage; nmo:messageId ?:token}"));

    query.bindValue(LAT("token"), messageToken);

    QScopedPointer<QSparqlResult> queryResult(connection().exec(query));

    if (!runBlockedQuery(queryResult.data())) // FIXIT
        return false;

    if (queryResult->first()) {
        QSparqlResultRow row = queryResult->current();
        if (!row.isEmpty())
            total = row.value(0).toInt();
    }

    return (total == 1);
}

void TrackerIOPrivate::requestCountMmsEvents()
{
    QSparqlQuery query(LAT(
            "SELECT COUNT(?message) "
            "WHERE {?message rdf:type nmo:MMSMessage}"));

    handleQuery(QSparqlQuery(query),
                this,
                "doCleanMmsGarbage",
                QVariant());
}

void TrackerIOPrivate::doCleanMmsGarbage(CommittingTransaction *transaction, QSparqlResult *result, QVariant arg)
{
    Q_UNUSED(transaction);
    Q_UNUSED(arg);
    if (result->first()) {
        QSparqlResultRow row = result->current();
        if (!row.isEmpty())
        {
            if(row.value(0).toInt() == 0)
            {
                getMmsDeleter(m_bgThread).cleanMmsPlace();
            }
        }
    }
}

void TrackerIOPrivate::checkAndDeletePendingMmsContent(QThread *backgroundThread)
{
    if (!m_messageTokenRefCount.isEmpty()) {
        for(MessageTokenRefCount::const_iterator it = m_messageTokenRefCount.begin();
            it != m_messageTokenRefCount.end();
            it++)
        {
            qDebug() << "[DELETER] Message: " << it.key() << "refcount:" << it.value();

            if (it.value() <= 0 && !it.key().isEmpty())
                getMmsDeleter(backgroundThread).deleteMessage(it.key());
        }

        m_messageTokenRefCount.clear();
    }
}

QSparqlConnection& TrackerIOPrivate::connection()
{
    if (!m_pConnection.hasLocalData()) {
        QSparqlConnectionOptions ops;
        ops.setDataReadyInterval(QSPARQL_DATA_READY_INTERVAL);
        m_pConnection.setLocalData(new QSparqlConnection(QSPARQL_DRIVER, ops));
    }

    return *m_pConnection.localData();
}

bool TrackerIOPrivate::checkPendingResult(QSparqlResult *result, bool destroyOnFinished)
{
    if (result->hasError()) {
        qWarning() << result->lastError().message();
        if (destroyOnFinished)
            delete result;
    }

    return !result->hasError();
}

bool TrackerIOPrivate::handleQuery(const QSparqlQuery &query,
                                   QObject *caller,
                                   const char *callback,
                                   QVariant arg)
{
    bool result = true;

    if (m_pTransaction) {
        m_pTransaction->d->addQuery(query, caller, callback, arg);
    } else {
        QSparqlResult *sResult = connection().exec(query);
        result = runBlockedQuery(sResult);
        if (callback) {
            // note: this can go recursive
            QMetaObject::invokeMethod(caller, callback,
                                      Q_ARG(CommittingTransaction *, 0),
                                      Q_ARG(QSparqlResult *, sResult),
                                      Q_ARG(QVariant, arg));
        } else {
            delete sResult;
        }
    }

    return result;
}

bool TrackerIOPrivate::runBlockedQuery(QSparqlResult *result)
{
    if (!checkPendingResult(result, false))
        return false;

    result->waitForFinished();

    if (!checkPendingResult(result, false))
        return false;

    return true;
}
