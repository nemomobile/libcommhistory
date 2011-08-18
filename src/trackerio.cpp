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
#include <tracker-sparql.h> // for syncTracker()

#include <QSparqlConnectionOptions>
#include <QSparqlConnection>
#include <QSparqlResult>
#include <QSparqlResultRow>
#include <QSparqlError>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusPendingCall>

#include <qtcontacts-tracker/phoneutils.h>

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

#define MAX_VARIABLES_IN_QUERY 100

#define NMO_ "http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#"

Q_GLOBAL_STATIC(TrackerIO, trackerIO)

namespace {
    QString escapeForQSparql(const QString &string) {
        QSparqlBinding binding;
        binding.setValue(string);
        return binding.toString();
    }
    QString encodeUri(const QUrl &uri) {
        return QString::fromAscii(uri.toEncoded());
    }
}

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
    if (!remoteUid.isNull() && remoteUid.isEmpty()) {
        // special case for hidden phone numbers
        constraints << QString(LAT("OPTIONAL { ?channel nmo:hasParticipant [ nco:hasContactMedium ?m ] . } "
                                   "FILTER(!BOUND(?m))"));
    } else if (!remoteUid.isEmpty()) {
        QString number = normalizePhoneNumber(remoteUid);
        if (number.isEmpty()) {
            constraints << QString(LAT("?channel nmo:hasParticipant [nco:hasIMAddress [nco:imID %1]] ."))
                          .arg(escapeForQSparql(remoteUid));
        } else {
            constraints << QString(LAT("?channel nmo:hasParticipant [nco:hasPhoneNumber [maemo:localPhoneNumber \"%1\"]] ."))
                          .arg(number.right(CommHistory::phoneNumberMatchLength()));
        }
    }
    if (!localUid.isEmpty()) {
        constraints << QString(LAT("FILTER(nie:subject(?channel) = %1) ")).arg(escapeForQSparql(localUid));
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
                  .arg(encodeUri(uri));

        m_contactCache.insert(uri, contact);
    }

    return contact;
}

void TrackerIOPrivate::ensureIMAddress(UpdateQuery &query,
                                       const QUrl &imAddressURI,
                                       const QString &imID)
{
    query.insertionRaw(imAddressURI, "rdf:type", LAT("nco:IMAddress"));
    query.insertion(imAddressURI, "nco:imID", imID);
}

void TrackerIOPrivate::ensurePhoneNumber(UpdateQuery &query,
                                         const QString &phoneIRI,
                                         const QString &phoneNumber,
                                         const QString &shortNumber)
{
    QString phoneNumberInsert =
        QString(LAT("INSERT SILENT { <%1> a nco:PhoneNumber ; "
                    "nco:phoneNumber \"%2\" ; "
                    "maemo:localPhoneNumber \"%3\" . }"))
        .arg(phoneIRI)
        .arg(phoneNumber)
        .arg(shortNumber);

    query.appendInsertion(phoneNumberInsert);
}

void TrackerIOPrivate::addSIPContact(UpdateQuery &query,
                                     const QUrl &subject,
                                     const char *predicate,
                                     const QString &accountPath,
                                     const QString &remoteUid,
                                     const QString &phoneNumber)
{
    // in SIP cases, save both the raw remoteUid
    // ("sip:01234567@voip.com") and the extracted phone number
    // (01234567) to the dummy contact for contact resolving.

    QUrl sipAddressIRI = uriForIMAddress(accountPath, remoteUid);
    QString phoneIRI = qctMakePhoneNumberIri(phoneNumber);

    // cache only used to avoid redundant inserts, ignore value
    if (!m_contactCache.contains(sipAddressIRI)) {
        ensureIMAddress(query, sipAddressIRI, remoteUid);
        QString shortNumber = makeShortNumber(phoneNumber);
        ensurePhoneNumber(query, phoneIRI, phoneNumber, shortNumber);
        m_contactCache.insert(sipAddressIRI, remoteUid);
    }

    QString contactInsert =
        QString(LAT("INSERT { <%1> %2 [ a nco:Contact ; "
                    "nco:hasIMAddress <%3> ; "
                    "nco:hasPhoneNumber <%4> ] }"))
        .arg(subject.toString())
        .arg(predicate)
        .arg(encodeUri(sipAddressIRI))
        .arg(phoneIRI);

    query.appendInsertion(contactInsert);
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
        ensureIMAddress(query, imAddressURI, imID);

        contact = QString(LAT("[rdf:type nco:Contact; nco:hasIMAddress <%2>]"))
                  .arg(encodeUri(imAddressURI));
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
    QString contactInsert;

    if (phoneNumber.isEmpty()) {
        contactInsert =
            QString(LAT("INSERT { <%1> %2 [ a nco:Contact ] }"))
            .arg(encodeUri(subject))
            .arg(predicate);
    } else {
        QString phoneIRI = qctMakePhoneNumberIri(phoneNumber);

        // cache only used to avoid redundant inserts, ignore value
        if (!m_contactCache.contains(phoneNumber)) {
            QString shortNumber = makeShortNumber(phoneNumber, normalizeFlags);
            ensurePhoneNumber(query, phoneIRI, phoneNumber, shortNumber);
            m_contactCache.insert(phoneNumber, phoneIRI);
        }

        contactInsert =
            QString(LAT("INSERT { <%1> %2 [ a nco:Contact ; nco:hasPhoneNumber <%3> ] } "))
            .arg(encodeUri(subject))
            .arg(predicate)
            .arg(phoneIRI);
    }

    query.appendInsertion(contactInsert);
}

void TrackerIOPrivate::addRemoteContact(UpdateQuery &query,
                                        const QUrl &subject,
                                        const char *predicate,
                                        const QString &localUid,
                                        const QString &remoteUid,
                                        PhoneNumberNormalizeFlags normalizeFlags)
{
    QRegExp sipRegExp("^sips?:(.*)@");

    QString phoneNumber;
    if (sipRegExp.indexIn(remoteUid) != -1) {
        phoneNumber = normalizePhoneNumber(sipRegExp.cap(1));
        if (!phoneNumber.isEmpty()) {
            return addSIPContact(query, subject, predicate, localUid, remoteUid, sipRegExp.cap(1));
        } else {
            return addIMContact(query, subject, predicate, localUid, remoteUid);
        }
    }

    if (remoteUid.isEmpty() || !normalizePhoneNumber(remoteUid, normalizeFlags).isEmpty()) {
        return addPhoneContact(query, subject, predicate, remoteUid, normalizeFlags);
    } else {
        return addIMContact(query, subject, predicate, localUid, remoteUid);
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
                            "nie:characterSet",
                            event.characterSet(),
                            modifyMode);
            break;
        case Event::Language:
            query.insertion(event.url(),
                            "nie:language",
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

    query.insertionSilent(
        QString(LAT("GRAPH <%1> { <%2> a nmo:CommunicationChannel }"))
        .arg(COMMHISTORY_GRAPH_CALL_CHANNEL)
        .arg(encodeUri(channelUri)));

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
            .arg(encodeUri(channelUri)));
    }

    query.insertion(eventSubject, "nmo:communicationChannel", channelUri);
}

void TrackerIOPrivate::setChannel(UpdateQuery &query, Event &event, int channelId, bool modify)
{
    QUrl channelUrl = Group::idToUrl(channelId);

    QString endTime = event.endTime().toUTC().toString(Qt::ISODate);
    QString startTime = event.startTime().toUTC().toString(Qt::ISODate);
    query.deletion(QString(LAT("DELETE {<%1> nmo:lastMessageDate ?d} "
                               "WHERE {"
                               "<%1> nmo:lastMessageDate ?d "
                               "OPTIONAL{SELECT ?lastMessage { ?lastMessage nmo:communicationChannel <%1>}"
                               " ORDER BY DESC(nmo:sentDate(?lastMessage)) DESC(tracker:id(?lastMessage)) LIMIT 1} "
                               "FILTER(!bound(?lastMessage) || nmo:sentDate(?lastMessage) < \"%2\"^^xsd:dateTime)}"))
                   .arg(channelUrl.toString())
                   .arg(startTime));
    query.insertionSilent(QString(LAT("<%1> nmo:lastMessageDate \"%2\"^^xsd:dateTime . "))
                          .arg(channelUrl.toString())
                          .arg(endTime));

    query.insertion(channelUrl,
                    "nie:contentLastModified",
                    event.endTime(),
                    true);
    query.insertion(event.url(),
                    "nmo:communicationChannel",
                    channelUrl,
                    modify);

    if (event.type() == Event::SMSEvent || event.type() == Event::MMSEvent) {
        query.insertion(channelUrl,
                        "nie:generator",
                        event.remoteUid(),
                        true);
    }

    QString phoneNumber = normalizePhoneNumber(event.remoteUid());
    if (!phoneNumber.isEmpty()) {
        query.deletion(channelUrl, "nmo:hasParticipant");
        addPhoneContact(query, channelUrl, "nmo:hasParticipant",
                        event.remoteUid(), NormalizeFlagRemovePunctuation);
    }
}

void TrackerIOPrivate::doUpdateGroupTimestamps(CommittingTransaction *transaction,
                                               QSparqlResult *result,
                                               QVariant arg)
{
    if (result && result->hasError()) {
        qCritical() << result->lastError().message();
        return;
    }

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

        addToTransactionOrRunQuery(transaction,
                                   QSparqlQuery(update.query(),
                                                QSparqlQuery::InsertStatement));
    }
}

void TrackerIOPrivate::updateGroupTimestamps(CommittingTransaction *transaction,
                                             QSparqlResult *result,
                                             QVariant arg)
{
    if (result && result->hasError()) {
        qCritical() << result->lastError().message();
        return;
    }

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
                    "(SELECT %1(?lastMessage) { ?lastMessage nmo:communicationChannel ?channel ; "
                    "   nmo:sentDate ?lastDate . } ORDER BY DESC(?lastDate) DESC(tracker:id(?lastMessage)))"
                    "(SELECT ?lastSuccessfulDate { "
                    " ?lastMessage nmo:communicationChannel ?channel ; "
                    " nmo:sentDate ?lastSuccessfulDate . "
                    " FILTER(nmo:isSent(?lastMessage) = true || "
                    "   nmo:isAnswered(?lastMessage) = true) "
                    "} ORDER BY DESC(?lastSuccessfulDate) DESC(tracker:id(?lastMessage)))"
                    "WHERE { "
                    " ?channel a nmo:CommunicationChannel . "
                    " FILTER(?channel = ?:channel) }"))
        .arg(timeProperty);

    QSparqlQuery query(timestampQuery);
    query.bindValue(LAT("channel"), QUrl(groupUri));

    addToTransactionOrRunQuery(transaction,
                               query,
                               this,
                               "doUpdateGroupTimestamps",
                               QVariant(groupUri));
}

bool TrackerIOPrivate::markGroupAsRead(const QString &channelIRI)
{
    QSparqlQuery query(LAT(
            "DELETE {?msg nmo:isRead ?r; nie:contentLastModified ?d}"
            "WHERE {?msg rdf:type nmo:Message; nmo:communicationChannel ?:channel;"
                   "nmo:isRead ?r; nie:contentLastModified ?d}"
            "INSERT {?msg nmo:isRead ?:read; nie:contentLastModified ?:date}"
            "WHERE {?msg rdf:type nmo:Message; nmo:communicationChannel ?:channel}"),
                       QSparqlQuery::InsertStatement);

    query.bindValue(LAT("channel"), channelIRI);
    query.bindValue(LAT("read"), true);
    //Need to update the contentModifiedTime as well so that NOS gets update with the updated time
    query.bindValue(LAT("date"), QDateTime::currentDateTime());

    return addToTransactionOrRunQuery(m_pTransaction, query);
}

void TrackerIO::recreateIds()
{
    qDebug() << Q_FUNC_INFO;

    // Read max event/group ids from tracker and reset IdSource.

    QSparqlQuery query("SELECT ?m { ?m a nmo:Message. FILTER(REGEX(?m, \"^(message|call):\")) } ORDER BY DESC(tracker:id(?m)) LIMIT 1");
    QSparqlResult *result = d->connection().syncExec(query);
    if (result->hasError()) {
        qCritical() << Q_FUNC_INFO << "Error querying message ids";
        delete result;
        return;
    }

    int maxMessageId = 0;
    int len = QString(QLatin1String("message:")).length();
    if (result->first()) {
        bool ok = false;
        int id = result->value(0).toString().mid(len).toInt(&ok);
        if (ok)
            maxMessageId = id;
    }
    delete result;

    QSparqlQuery groupQuery("SELECT ?c { ?c a nmo:CommunicationChannel. FILTER(REGEX(?c, \"^conversation:\")) } ORDER BY DESC(tracker:id(?c)) LIMIT 1");
    result = d->connection().syncExec(groupQuery);
    if (result->hasError()) {
        qCritical() << Q_FUNC_INFO << "Error querying group ids";
        delete result;
        return;
    }

    int maxGroupId = 0;
    len = QString(QLatin1String("conversation:")).length();
    if (result->first()) {
        bool ok = false;
        int id = result->value(0).toString().mid(len).toInt(&ok);
        if (ok)
            maxGroupId = id;
    }
    delete result;

    d->m_IdSource.setNextEventId(maxMessageId + 1);
    d->m_IdSource.setNextGroupId(maxGroupId + 1);

    qDebug() << Q_FUNC_INFO << "max event id =" << maxMessageId << ", group id =" << maxGroupId;
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

    if (group.remoteUids().isEmpty()) {
        qWarning() << "No remote uid for group";
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

    if (remoteUid.isEmpty() || !phoneNumber.isEmpty()) {
        d->addPhoneContact(query, channelSubject, "nmo:hasParticipant", remoteUid,
                           NormalizeFlagRemovePunctuation);
        query.insertion(channelSubject,
                        "nie:generator",
                        remoteUid);
    } else {
        d->addIMContact(query, channelSubject, "nmo:hasParticipant",
                        group.localUid(), remoteUid);
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
        if (d->m_pTransaction)
            connect(d->m_pTransaction,
                    SIGNAL(finished()),
                    d,
                    SLOT(requestMmsEventsCount()),
                    Qt::UniqueConnection);
    }

    QString query(LAT("DELETE {?:uri a rdfs:Resource}"));

    switch (event.type()) {
    case Event::SMSEvent:
        // delete vcard
        query = LAT("DELETE {?vcardFile a rdfs:Resource} "
                    "WHERE {?:uri nmo:fromVCard ?vcardFile}")
                + query;
        break;
    case Event::CallEvent:
        // delete empty call groups
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
        break;
    case Event::MMSEvent:
        // delete message parts and header
        query = LAT("DELETE  {?part rdf:type rdfs:Resource}"
                    "WHERE {?:uri nmo:mmsHasContent [nie:hasPart ?part]}"
                    "DELETE {?content a rdfs:Resource} "
                    "WHERE {?:uri nmo:mmsHasContent ?content}"
                    "DELETE {?header a rdfs:Resource} "
                    "WHERE {?:uri nmo:messageHeader ?header}")
                    + query;
        break;
    default:
        // nothing to be done for other event types
        break;
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

bool TrackerIOPrivate::queryMmsTokensForGroups(QList<int> groupIds)
{
    QListIterator<int> i(groupIds);
    QList<int> batchGroups;

    // query in batches to avoid "too many variables" error
    while (i.hasNext()) {
        while (batchGroups.size() < MAX_VARIABLES_IN_QUERY && i.hasNext())
            batchGroups << i.next();

        QStringList groups;
        foreach (int groupId, batchGroups) {
            groups.append(QString(LAT("<%1>")).arg(Group::idToUrl(groupId).toString()));
        }

        QSparqlQuery query(QString(LAT(
                                       "SELECT DISTINCT nmo:messageId(?message) "
                                       "WHERE {"
                                       "?message rdf:type nmo:MMSMessage; nmo:communicationChannel ?channel "
                                       "FILTER(?channel IN (%1))}")).arg(groups.join(LAT(","))));

        if (!handleQuery(query,
                          this,
                          "mmsTokensReady",
                          QVariant::fromValue(batchGroups)))
            return false;
        batchGroups.clear();
    }

    return true;
}

bool TrackerIOPrivate::doDeleteGroups(CommittingTransaction *transaction,
                                      QList<int> groupIds,
                                      bool deleteMessages,
                                      bool cleanMmsParts)
{
    qDebug() << Q_FUNC_INFO << groupIds << deleteMessages;

    UpdateQuery update;
    QStringList groups;
    foreach (int groupId, groupIds) {
        groups.append(QString(LAT("<%1>")).arg(Group::idToUrl(groupId).toString()));
    }

    if (deleteMessages) {
        if (cleanMmsParts) {
            // delete mms parts resources
            update.deletion(QString(LAT("DELETE  {?part rdf:type rdfs:Resource}"
                                        "WHERE {?msg rdf:type nmo:MMSMessage; "
                                        "nmo:mmsHasContent [nie:hasPart ?part]; "
                                        "nmo:communicationChannel ?channel "
                                        "FILTER(?channel IN (%1))}"))
                            .arg(groups.join(LAT(","))));

            update.deletion(QString(LAT("DELETE {?content rdf:type rdfs:Resource}"
                                        "WHERE {?msg rdf:type nmo:MMSMessage; "
                                        "nmo:mmsHasContent ?content; "
                                        "nmo:communicationChannel ?channel "
                                        "FILTER(?channel IN (%1))}"))
                            .arg(groups.join(LAT(","))));

            update.deletion(QString(LAT("DELETE {?header rdf:type rdfs:Resource}"
                                        "WHERE {?msg rdf:type nmo:MMSMessage; "
                                        "nmo:messageHeader ?header; "
                                        "nmo:communicationChannel ?channel "
                                        "FILTER(?channel IN (%1))}"))
                            .arg(groups.join(LAT(","))));
        }
        // delete vcard resources
        update.deletion(QString(LAT("DELETE {?vcardFile rdf:type rdfs:Resource}"
                                    "WHERE {?msg rdf:type nmo:SMSMessage; "
                                    "nmo:fromVCard ?vcardFile; "
                                    "nmo:communicationChannel ?channel "
                                    "FILTER(?channel IN (%1))}"))
                        .arg(groups.join(LAT(","))));

        update.deletion(QString(LAT("DELETE {?msg rdf:type rdfs:Resource}"
                                    "WHERE {?msg rdf:type nmo:Message; "
                                    "nmo:communicationChannel ?channel "
                                    "FILTER(?channel IN (%1))}"))
                        .arg(groups.join(LAT(","))));
    }

    update.deletion(QString(LAT("DELETE {?channel rdf:type rdfs:Resource}"
                                "WHERE {?channel rdf:type nmo:CommunicationChannel "
                                "FILTER(?channel IN (%1))}"))
                    .arg(groups.join(LAT(","))));


    QSparqlQuery query(update.query(), QSparqlQuery::DeleteStatement);

    if (transaction) {
        connect(transaction,
                SIGNAL(finished()),
                this,
                SLOT(requestMmsEventsCount()),
                Qt::UniqueConnection);
    }

    if (transaction) {
        transaction->d->addQuery(query);
        return true;
    }

    return handleQuery(query);
}

void TrackerIOPrivate::mmsTokensReady(CommittingTransaction *transaction,
                                      QSparqlResult *result,
                                      QVariant arg)
{
    Q_ASSERT(transaction);

    QList<int> groupIds = arg.value<QList<int> >();
    bool hasMms = false;

    while (result->next()) {
        QSparqlResultRow row = result->current();

        if (row.isEmpty()) {
            qWarning() << Q_FUNC_INFO << "Empty row";
            continue;
        }

        QString messageToken(row.value(0).toString());

        if (!messageToken.isEmpty()) {
            m_mmsTokens.insert(messageToken);
            hasMms = true;
        }
    }

    doDeleteGroups(transaction,
                   groupIds,
                   true,
                   hasMms);
}

bool TrackerIO::deleteGroup(int groupId, bool deleteMessages, QThread *backgroundThread)
{
    return deleteGroups(QList<int>() << groupId, deleteMessages, backgroundThread);
}

bool TrackerIO::deleteGroups(QList<int> groupIds, bool deleteMessages, QThread *backgroundThread)
{
    qDebug() << Q_FUNC_INFO << groupIds << deleteMessages << backgroundThread;

    d->m_bgThread = backgroundThread;

    if (deleteMessages)
        return d->queryMmsTokensForGroups(groupIds);

    return d->doDeleteGroups(d->m_pTransaction, groupIds, deleteMessages, false);
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
    return d->markGroupAsRead(Group::idToUrl(groupId).toString());
}

bool TrackerIO::markAsReadCallGroup(Event &event)
{
    return d->markGroupAsRead(d->makeCallGroupURI(event));
}

bool TrackerIO::markAsReadAll(Event::EventType eventType)
{
    qDebug() << __FUNCTION__ << eventType;

    QString query("DELETE {?e nmo:isRead ?r; nie:contentLastModified ?d}"
                  "WHERE {?e rdf:type ?:eventType; nmo:isRead ?r; nie:contentLastModified ?d}"
                  "INSERT {?e nmo:isRead true; nie:contentLastModified ?:date}"
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
        break;
    case Event::MMSEvent:
        eventTypeUrl = QUrl(LAT(NMO_ "MMSMessage"));
        break;
    default:
        qWarning() << __FUNCTION__ << "Unsupported type" << eventType;
        return false;
    }

    QSparqlQuery markAllQuery(query, QSparqlQuery::InsertStatement);
    markAllQuery.bindValue(LAT("eventType"), eventTypeUrl);
    markAllQuery.bindValue(LAT("date"), QDateTime::currentDateTime());

    return d->handleQuery(markAllQuery);
}

void TrackerIO::transaction(bool syncOnCommit)
{
    Q_ASSERT(d->m_pTransaction == 0);

    d->syncOnCommit = syncOnCommit;
    d->m_pTransaction = new CommittingTransaction(this);
    d->m_mmsTokens.clear();
}

CommittingTransaction* TrackerIO::commit(bool isBlocking)
{
    Q_ASSERT(d->m_pTransaction);

    CommittingTransaction *returnTransaction = 0;

    if (isBlocking) {
        d->m_pTransaction->run(d->connection(), true);
        if (d->syncOnCommit)
            d->syncTracker();
        delete d->m_pTransaction;
    } else {
        if (!d->m_pTransaction->d->isEmpty()) {
            if (d->syncOnCommit)
                connect(d->m_pTransaction, SIGNAL(finished()),
                        d, SLOT(syncTracker()));
            d->m_pendingTransactions.enqueue(d->m_pTransaction);
            d->runNextTransaction();
            // if m_pTransaction is not in pending transactions,
            // it's failed right away
            if (d->m_pendingTransactions.contains(d->m_pTransaction))
                returnTransaction = d->m_pTransaction;
        } else {
            qWarning() << Q_FUNC_INFO << "Empty transaction committing";
            delete d->m_pTransaction;
        }

    }

    d->m_contactCache.clear();
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
        t->deleteLater(); // allow other finished() slots to be invoked
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
    d->m_mmsTokens.clear(); // Clear cache to avoid deletion after rollback
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
        if (d->m_pTransaction)
            connect(d->m_pTransaction,
                    SIGNAL(finished()),
                    d,
                    SLOT(requestMmsEventsCount()),
                    Qt::UniqueConnection);
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

void TrackerIOPrivate::requestMmsEventsCount()
{
    qDebug() << Q_FUNC_INFO;
    QSparqlQuery query(LAT(
            "SELECT COUNT(?message) "
            "WHERE {?message rdf:type nmo:MMSMessage}"));

    handleQuery(QSparqlQuery(query),
                this,
                "mmsCountReady");
}

void TrackerIOPrivate::mmsCountReady(CommittingTransaction *transaction,
                                     QSparqlResult *result,
                                     QVariant arg)
{
    Q_UNUSED(arg);

    bool cleanMms = false;

    if (result->first()) {
        QSparqlResultRow row = result->current();
        cleanMms = !row.isEmpty() && row.value(0).toInt() == 0;
    }

    if (cleanMms) {
        // no mms in tracker, delete all mms content
        // explicitly delete "old" mms bypassing folder time check in cleanMmsPlace()
        foreach (QString token, m_mmsTokens) {
            getMmsDeleter(m_bgThread).deleteMessage(token);
        }
        getMmsDeleter(m_bgThread).cleanMmsPlace();

    } else if (!m_mmsTokens.isEmpty()) {
        QSetIterator<QString> i(m_mmsTokens);
        QStringList batchTokens;

        while (i.hasNext()) {
            while (batchTokens.size() < MAX_VARIABLES_IN_QUERY && i.hasNext())
                batchTokens << i.next();

            QStringList mmsTokens;

            foreach (QString token, batchTokens) {
                mmsTokens.append(QString(LAT("\"%1\"")).arg(token));
            }

            QSparqlQuery query(QString(LAT(
                                           "SELECT DISTINCT ?token "
                                           "WHERE {"
                                           "?message rdf:type nmo:MMSMessage; nmo:messageId ?token "
                                           "FILTER(?token IN (%1))}")).arg(mmsTokens.join(LAT(","))));

            addToTransactionOrRunQuery(transaction,
                                       query,
                                       this,
                                       "deleteMmsContent",
                                       QVariant::fromValue(batchTokens));
            batchTokens.clear();
        }
    }
    m_mmsTokens.clear();
}

void TrackerIOPrivate::deleteMmsContent(CommittingTransaction *transaction,
                                        QSparqlResult *result,
                                        QVariant arg)
{
    Q_UNUSED(transaction);

    QStringList mmsTokens = arg.value<QStringList>();
    while (result->next()) {
        QSparqlResultRow row = result->current();

        if (row.isEmpty()) {
            qWarning() << Q_FUNC_INFO << "Empty row";
            continue;
        }

        QString messageToken(row.value(0).toString());

        if (!messageToken.isEmpty()) {
            qDebug() << Q_FUNC_INFO << "DONT DELETE " << messageToken;
            mmsTokens.removeOne(messageToken);
        }
    }

    foreach (QString token, mmsTokens) {
        getMmsDeleter(m_bgThread).deleteMessage(token);
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
        qCritical() << result->lastError().message();
        if (destroyOnFinished)
            delete result;
    }

    return !result->hasError();
}

bool TrackerIOPrivate::addToTransactionOrRunQuery(CommittingTransaction *transaction,
                                                  const QSparqlQuery &query,
                                                  QObject *caller,
                                                  const char *callback,
                                                  QVariant argument)
{
    bool result = true;

    if (transaction) {
        transaction->d->addQuery(query, caller, callback, argument);
    } else {
        QSparqlResult *sResult = connection().exec(query);
        result = runBlockedQuery(sResult);
        if (callback) {
            // note: this can go recursive
            QMetaObject::invokeMethod(caller, callback,
                                      Q_ARG(CommittingTransaction *, 0),
                                      Q_ARG(QSparqlResult *, sResult),
                                      Q_ARG(QVariant, argument));
        }
        sResult->deleteLater();
    }

    return result;
}

bool TrackerIOPrivate::handleQuery(const QSparqlQuery &query,
                                   QObject *caller,
                                   const char *callback,
                                   QVariant argument)
{
    return addToTransactionOrRunQuery(m_pTransaction,
                                      query,
                                      caller,
                                      callback,
                                      argument);
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


void TrackerIOPrivate::syncTracker()
{
    QDBusMessage syncCall = QDBusMessage::createMethodCall(LAT(TRACKER_DBUS_SERVICE),
                                                           LAT(TRACKER_DBUS_OBJECT_RESOURCES),
                                                           LAT(TRACKER_DBUS_INTERFACE_RESOURCES),
                                                           LAT("Sync"));
    QDBusConnection::sessionBus().asyncCall(syncCall);
}
