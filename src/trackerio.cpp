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

#include <QtTracker/Tracker>
#include <QtTracker/ontologies/nco.h>
#include <QtTracker/ontologies/nmo.h>
#include <QtTracker/ontologies/nie.h>
#include <QtTracker/ontologies/nfo.h>
#include <QtTracker/ontologies/rdfs.h>

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

#include "trackerio_p.h"
#include "trackerio.h"

using namespace CommHistory;
using namespace SopranoLive;

#define LAT(STR) QLatin1String(STR)

#define QSPARQL_DRIVER QLatin1String("QTRACKER_DIRECT")
#define QSPARQL_DATA_READY_INTERVAL 25

#define NMO_ "http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#"

// NOTE projections in the query should have same order as Group::Property
#define GROUP_QUERY LAT( \
"SELECT ?_channel" \
"        ?_subject" \
"     nie:generator(?_channel)" \
"     nie:identifier(?_channel)" \
"     nie:title(?_channel)" \
"  ?_lastDate" \
"(SELECT COUNT(?_total_messages)" \
" WHERE {" \
"  ?_total_messages nmo:communicationChannel ?_channel;" \
"                   nmo:isDeleted \"false\" ." \
" }" \
")" \
"(SELECT COUNT(?_total_unread_messages)" \
" WHERE {" \
"  ?_total_unread_messages nmo:communicationChannel ?_channel;" \
"                   nmo:isRead \"false\";" \
"                   nmo:isDeleted \"false\"" \
" }" \
")" \
"(SELECT COUNT(?_total_sent_messages)" \
" WHERE {" \
"  ?_total_sent_messages nmo:communicationChannel ?_channel;" \
"                   nmo:isSent \"true\";" \
"                   nmo:isDeleted \"false\"" \
" }" \
")" \
"  ?_lastMessage" \
"  tracker:id(?_contact_1)" \
"  fn:concat(tracker:coalesce(nco:nameGiven(?_contact_1), \'\'), \'\\u001e\'," \
"            tracker:coalesce(nco:nameFamily(?_contact_1), \'\'), \'\\u001e\'," \
"            tracker:coalesce(?_imNickname, \'\'))" \
"   tracker:coalesce(nmo:messageSubject(?_lastMessage)," \
"                    nie:plainTextContent(?_lastMessage))" \
"     nfo:fileName(nmo:fromVCard(?_lastMessage))" \
"     rdfs:label(nmo:fromVCard(?_lastMessage))" \
"     rdf:type(?_lastMessage)" \
"     nmo:deliveryStatus(?_lastMessage)" \
"  ?_lastModified " \
"WHERE" \
"{" \
"  {" \
"    SELECT ?_channel ?_subject ?_lastDate ?_lastModified" \
"         ( SELECT ?_message" \
"  WHERE {" \
"    ?_message nmo:communicationChannel ?_channel ;" \
"              nmo:isDeleted \"false\" ." \
"  }" \
"    ORDER BY DESC(nmo:receivedDate(?_message)) DESC(tracker:id(?_message)) " \
"    LIMIT 1)" \
"      AS ?_lastMessage" \
"         ( SELECT ?_contact" \
"  WHERE {" \
"        ?_contact rdf:type nco:PersonContact ." \
"      {" \
"        ?_channel nmo:hasParticipant [nco:hasIMAddress ?_12 ] ." \
"        ?_contact nco:hasAffiliation [nco:hasIMAddress ?_12 ]" \
"      }" \
"      UNION" \
"      {" \
"        ?_channel nmo:hasParticipant [nco:hasPhoneNumber [maemo:localPhoneNumber ?_16 ]] ." \
"        ?_contact nco:hasAffiliation [nco:hasPhoneNumber [maemo:localPhoneNumber ?_16 ]]" \
"      }" \
"  })" \
"" \
"      AS ?_contact_1" \
"         ( SELECT ?_13" \
"  WHERE {" \
"" \
"    ?_channel nmo:hasParticipant [nco:hasIMAddress [nco:imNickname ?_13 ]]" \
"  })" \
"" \
"      AS ?_imNickname" \
"    WHERE" \
"    {" \
"      ?_channel rdf:type nmo:CommunicationChannel; " \
"      nie:subject ?_subject ; " \
"      nmo:lastMessageDate ?_lastDate ; " \
"      nie:contentLastModified ?_lastModified ." \
"      %1"\
"    }" \
"  }" \
"}" \
"  ORDER BY DESC(?_lastDate)" \
)

Q_GLOBAL_STATIC(TrackerIO, trackerIO)

TrackerIOPrivate::TrackerIOPrivate(TrackerIO *parent)
    : q(parent),
    m_pTransaction(0),
    m_service(::tracker()),
    m_MmsContentDeleter(0),
    m_bgThread(0)
{
}

TrackerIOPrivate::~TrackerIOPrivate()
{
    foreach(CommittingTransaction* t, m_pendingTransactions)
        t->deleteLater();

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

int TrackerIO::nextGroupId()
{
    return d->m_IdSource.nextGroupId();
}

void TrackerIO::addMessagePropertiesToQuery(SopranoLive::RDFSelect &query,
                                            const Event::PropertySet &propertyMask,
                                            SopranoLive::RDFVariable &message)
{
    if (propertyMask.contains(Event::GroupId))
        query.addColumn(LAT("channel"), message.function<nmo::communicationChannel>());

    if (propertyMask.contains(Event::Direction)
        || propertyMask.contains(Event::LocalUid)
        || propertyMask.contains(Event::RemoteUid)
        || propertyMask.contains(Event::Status))
        query.addColumn(LAT("isSent"), message.function<nmo::isSent>());

    if (propertyMask.contains(Event::IsDraft))
        query.addColumn(LAT("isDraft"), message.function<nmo::isDraft>());

    if (propertyMask.contains(Event::IsRead))
        query.addColumn(LAT("isRead"), message.function<nmo::isRead>());

    if (propertyMask.contains(Event::IsMissedCall))
        query.addColumn(LAT("isAnswered"), message.function<nmo::isAnswered>());

    if (propertyMask.contains(Event::IsEmergencyCall))
        query.addColumn(LAT("isEmergency"), message.function<nmo::isEmergency>());

    if (propertyMask.contains(Event::IsDeleted))
        query.addColumn(LAT("isDeleted"), message.function<nmo::isDeleted>());

    if (propertyMask.contains(Event::MessageToken))
        query.addColumn(LAT("messageId"), message.function<nmo::messageId>());

    if (propertyMask.contains(Event::MmsId))
        query.addColumn(LAT("mmsId"), message.function<nmo::mmsId>());

    if (propertyMask.contains(Event::ParentId))
        // TODO: smsId is being used as parentId, fix naming
        query.addColumn(LAT("smsId"), message.function<nmo::phoneMessageId>());

    if (propertyMask.contains(Event::StartTime))
        query.addColumn(LAT("sentDate"), message.function<nmo::sentDate>());

    if (propertyMask.contains(Event::EndTime))
        query.addColumn(LAT("receivedDate"), message.function<nmo::receivedDate>());

    if (propertyMask.contains(Event::LastModified))
        query.addColumn(LAT("lastModified"), message.function<nie::contentLastModified>());

    if (propertyMask.contains(Event::Subject))
        query.addColumn(LAT("messageSubject"), message.function<nmo::messageSubject>());

    if (propertyMask.contains(Event::FreeText))
        query.addColumn(LAT("textContent"), message.function<nie::plainTextContent>());

    if (propertyMask.contains(Event::Status))
        query.addColumn(LAT("deliveryStatus"), message.function<nmo::deliveryStatus>());

    if (propertyMask.contains(Event::ReportDelivery))
        query.addColumn(LAT("reportDelivery"), message.function<nmo::reportDelivery>());

    if (propertyMask.contains(Event::ReportRead))
        query.addColumn(LAT("sentWithReportRead"), message.function<nmo::sentWithReportRead>());

    if (propertyMask.contains(Event::ReportReadRequested))
        query.addColumn(LAT("mustAnswerReportRead"), message.function<nmo::mustAnswerReportRead>());
    if (propertyMask.contains(Event::ReportReadStatus))
        query.addColumn(LAT("reportReadStatus"), message.function<nmo::reportReadStatus>());

    if (propertyMask.contains(Event::ValidityPeriod))
        query.addColumn("validityPeriod", message.function<nmo::validityPeriod>());

    if (propertyMask.contains(Event::BytesReceived))
        query.addColumn(LAT("contentSize"), message.function<nie::contentSize>());

    // TODO: nie:url doesn't work anymore because nmo:PhoneMessage is
    // not a subclass of nie:DataObject. nie:generator is a harmless
    // workaround, but should we use something like a dummy nie:links
    // dataobject (preferably with anon blank nodes)?
    if (propertyMask.contains(Event::ContentLocation))
        query.addColumn(LAT("contentLocation"), message.function<nie::generator>());

    //additional parameters for sms
    if (propertyMask.contains(Event::FromVCardFileName)
        || propertyMask.contains(Event::FromVCardLabel)) {
        RDFVariable fromVCard = message.function<nmo::fromVCard>();
        query.addColumn(LAT("fromVCardName"), fromVCard.function<nfo::fileName>());
        query.addColumn(LAT("fromVCardLabel"), fromVCard.function<rdfs::label>());
    }

    if (propertyMask.contains(Event::Encoding))
        query.addColumn(LAT("encoding"), message.function<nmo::encoding>());

    if (propertyMask.contains(Event::CharacterSet))
        query.addColumn(LAT("characterSet"), message.function<nie::characterSet>());

    if (propertyMask.contains(Event::Language))
        query.addColumn(LAT("language"), message.function<nie::language>());
}

void TrackerIO::prepareMessageQuery(RDFSelect &messageQuery, RDFVariable &message,
                                    const Event::PropertySet &propertyMask,
                                    QUrl communicationChannel)
{
    RDFSelect query;

    RDFVariable contact;
    RDFVariable outerImAddress;

    RDFVariable outerMessage = query.newColumn(LAT("message"));
    RDFVariable date = query.newColumn(LAT("date"));

    RDFSubSelect subSelect;
    RDFVariable subMessage = subSelect.newColumnAs(outerMessage);
    subMessage = message; // copy constraints from argument
    date = subMessage.property<nmo::receivedDate>();
    RDFVariable subDate = subSelect.newColumnAs(date);

    query.addColumn(LAT("type"), outerMessage.function<rdf::type>());

    if (propertyMask.contains(Event::ContactId)
        || propertyMask.contains(Event::ContactName)) {
        query.addColumn(LAT("contact"), contact);
        query.addColumn(LAT("imAddress"), outerImAddress);
    }

    if (propertyMask.contains(Event::LocalUid)
        || propertyMask.contains(Event::RemoteUid)) {
        RDFVariable from = query.newColumn(LAT("from"));
        RDFVariable to = query.newColumn(LAT("to"));
        RDFVariable subFrom = subSelect.newColumnAs(from);
        RDFVariable subTo = subSelect.newColumnAs(to);
        from = subMessage.property<nmo::from>().property<nco::hasContactMedium>();
        to = subMessage.property<nmo::to>().property<nco::hasContactMedium>();
    }

    if (!communicationChannel.isEmpty()) {
        // optimized query for one p2p conversation - tie all messages to the channel
        subMessage.property<nmo::communicationChannel>(communicationChannel);
    }

    addMessagePropertiesToQuery(query, propertyMask, outerMessage);

    if (propertyMask.contains(Event::ContactId)
        || propertyMask.contains(Event::ContactName)) {
        RDFSubSelect contactSelect;
        RDFVariable subContact = contactSelect.newColumnAs(contact);

        RDFVariable channel = contactSelect.variable(LAT("channel"));
        RDFVariableList contacts = subContact.unionChildren(2);

        // by IM address...
        contacts[0].isOfType<nco::PersonContact>();
        RDFVariable imChannel = contacts[0].variable(channel);
        RDFVariable imParticipant;
        RDFVariable imAddress;
        if (communicationChannel.isEmpty()) {
            imChannel = subMessage.property<nmo::communicationChannel>();
            imParticipant = imChannel.property<nmo::hasParticipant>();
            imAddress = imParticipant.property<nco::hasIMAddress>();
        } else {
            imChannel = communicationChannel;
            imParticipant = imChannel.property<nmo::hasParticipant>();
            imAddress = imParticipant.property<nco::hasIMAddress>();
        }
        RDFVariable imAffiliation = contacts[0].property<nco::hasAffiliation>();
        imAffiliation.property<nco::hasIMAddress>() = imAddress;

        // affiliation (work number)
        contacts[1].isOfType<nco::PersonContact>();
        RDFVariable affChannel = contacts[1].variable(channel);
        RDFVariable affParticipant;
        if (communicationChannel.isEmpty()) {
            affChannel = subMessage.property<nmo::communicationChannel>();
            affParticipant = affChannel.property<nmo::hasParticipant>();
        } else {
            affChannel = communicationChannel;
            affParticipant = affChannel.property<nmo::hasParticipant>();
        }
        RDFVariable affPhoneNumber = affParticipant.property<nco::hasPhoneNumber>()
            .property<maemo::localPhoneNumber>();
        contacts[1].property<nco::hasAffiliation>()
            .property<nco::hasPhoneNumber>().property<maemo::localPhoneNumber>() = affPhoneNumber;

        subSelect.addColumnAs(contactSelect.asExpression(), contact);

        RDFSubSelect imNicknameSubSelect;
        RDFVariable messageChannel = imNicknameSubSelect.variable(LAT("messageChannel"));
        if (communicationChannel.isEmpty()) {
            messageChannel = imNicknameSubSelect.variable(subMessage)
                .property<nmo::communicationChannel>();
        } else {
            messageChannel = communicationChannel;
        }
        RDFVariable imNickAddress = messageChannel.property<nmo::hasParticipant>()
            .property<nco::hasIMAddress>();
        imNicknameSubSelect.addColumn(LAT("imNickname"), imNickAddress);
        subSelect.addColumnAs(imNicknameSubSelect.asExpression(), outerImAddress);

        RDFVariable idFilter = contact.filter(LAT("tracker:id"));
        query.addColumn(LAT("contactId"), idFilter);
        query.addColumn(LAT("contactFirstName"), contact.function<nco::nameGiven>());
        query.addColumn(LAT("contactLastName"), contact.function<nco::nameFamily>());
        query.addColumn(LAT("imNickname"), outerImAddress.function<nco::imNickname>());
    }

    query.orderBy(date, false);
    // enforce secondary sorting in the order of message saving
    // tracker::id() used instead of plain message uri for performance reason
    RDFVariable msgTrackerId = outerMessage.filter(LAT("tracker:id"));
    msgTrackerId.metaEnableStrategyFlags(RDFStrategy::IdentityColumn);
    query.addColumn(msgTrackerId);
    query.orderBy(msgTrackerId, false);

    messageQuery = query;
}

void TrackerIO::prepareMUCQuery(RDFSelect &messageQuery, RDFVariable &message,
                                const Event::PropertySet &propertyMask,
                                QUrl communicationChannel)
{
    RDFSelect query;

    RDFVariable contact;

    RDFVariable outerMessage = query.newColumn(LAT("message"));
    RDFVariable date = query.newColumn(LAT("date"));

    RDFSubSelect subSelect;
    RDFVariable subMessage = subSelect.newColumnAs(outerMessage);
    subMessage = message; // copy constraints from argument
    date = subMessage.property<nmo::receivedDate>();
    RDFVariable subDate = subSelect.newColumnAs(date);

    RDFVariable outerAddress;
    RDFVariable targetAddress;

    query.addColumn(LAT("type"), outerMessage.function<rdf::type>());

    if (propertyMask.contains(Event::ContactId)
        || propertyMask.contains(Event::ContactName)) {
        query.addColumn(LAT("contact"), contact);
    }

    if (propertyMask.contains(Event::LocalUid)
        || propertyMask.contains(Event::RemoteUid)) {
        RDFVariable from = query.newColumn(LAT("from"));
        RDFVariable to = query.newColumn(LAT("to"));
        RDFVariable subFrom = subSelect.newColumnAs(from);
        RDFVariable subTo = subSelect.newColumnAs(to);
        from = subMessage.property<nmo::from>().property<nco::hasContactMedium>();
        to = subMessage.property<nmo::to>().property<nco::hasContactMedium>();
    }

    if (propertyMask.contains(Event::ContactId)
        || propertyMask.contains(Event::ContactName)
        || propertyMask.contains(Event::LocalUid)
        || propertyMask.contains(Event::RemoteUid)) {

        outerAddress = query.newColumn(LAT("targetAddress"));
        targetAddress = subSelect.newColumnAs(outerAddress);

        RDFVariable target = subSelect.variable(LAT("target"));
        RDFVariableList fromToUnion = subMessage.unionChildren(2);
        fromToUnion[0].property<nmo::isSent>(LiteralValue(false));
        fromToUnion[0].property<nmo::from>(target);
        fromToUnion[1].property<nmo::isSent>(LiteralValue(true));
        fromToUnion[1].property<nmo::to>(target);
        target.property<nco::hasIMAddress>(targetAddress);
    }

    subMessage.property<nmo::communicationChannel>(communicationChannel);

    addMessagePropertiesToQuery(query, propertyMask, outerMessage);

    if (propertyMask.contains(Event::ContactId)
        || propertyMask.contains(Event::ContactName)) {
        RDFSubSelect contactSelect;
        RDFVariable subContact = contactSelect.newColumnAs(contact);
        subContact.isOfType<nco::PersonContact>();
        RDFVariable imAffiliation = subContact.property<nco::hasAffiliation>();
        imAffiliation.property<nco::hasIMAddress>(targetAddress);

        subSelect.addColumnAs(contactSelect.asExpression(), contact);

        RDFVariable idFilter = contact.filter(LAT("tracker:id"));
        query.addColumn(LAT("contactId"), idFilter);
        query.addColumn(LAT("contactFirstName"), contact.function<nco::nameGiven>());
        query.addColumn(LAT("contactLastName"), contact.function<nco::nameFamily>());
        query.addColumn(LAT("imNickname"), outerAddress.function<nco::imNickname>());
    }

    query.orderBy(date, false);
    // enforce secondary sorting in the order of message saving
    // tracker::id() used instead of plain message uri for performance reason
    RDFVariable msgTrackerId = outerMessage.filter(LAT("tracker:id"));
    msgTrackerId.metaEnableStrategyFlags(RDFStrategy::IdentityColumn);
    query.addColumn(msgTrackerId);
    query.orderBy(msgTrackerId, false);

    messageQuery = query;
}

void TrackerIO::prepareCallQuery(RDFSelect &callQuery, RDFVariable &call,
                                 const Event::PropertySet &propertyMask)
{
    RDFSelect query;

    RDFVariable contact;
    RDFVariable outerCall = query.newColumn(LAT("call"));
    RDFVariable date = query.newColumn(LAT("date"));
    RDFVariable from;
    RDFVariable to;

    RDFSubSelect subSelect;
    RDFVariable subCall = subSelect.newColumnAs(outerCall);
    subCall = call; // copy constraints
    date = subCall.property<nmo::sentDate>();
    RDFVariable subDate = subSelect.newColumnAs(date);

    query.addColumn(LAT("type"), outerCall.function<rdf::type>());

    if (propertyMask.contains(Event::LocalUid)
        || propertyMask.contains(Event::RemoteUid)
        || propertyMask.contains(Event::Direction)) {
        from = query.newColumn(LAT("from"));
        to = query.newColumn(LAT("to"));
        RDFVariable subFrom = subSelect.newColumnAs(from);
        RDFVariable subTo = subSelect.newColumnAs(to);
        from = subCall.property<nmo::from>().property<nco::hasContactMedium>();
        to = subCall.property<nmo::to>().property<nco::hasContactMedium>();
    }

    if (propertyMask.contains(Event::ContactId)
        || propertyMask.contains(Event::ContactName)) {
        RDFSubSelect contactSelect;
        RDFVariable subContact = contactSelect.newColumnAs(contact);
        RDFVariableList contacts = subContact.unionChildren(2);

        /*
          (contact -> nco:hasAffiliation -> nco:hasIMAddress) ==
          (call nmo:from|nmo:to -> nco:hasIMAddress)
        */
        contacts[0].isOfType<nco::PersonContact>();
        RDFVariable affCall = contacts[0].variable(subCall);
        RDFVariable affAddress = contacts[0].variable(LAT("imAddress"));
//        RDFVariable affIMAddress = contacts[0].property<nco::hasIMAddress>();
        RDFVariable affiliation;
        contacts[0].property<nco::hasAffiliation>(affiliation);
        affiliation.property<nco::hasIMAddress>(affAddress);

        RDFPattern affFrom = contacts[0].pattern().child();
        RDFPattern affTo = contacts[0].pattern().child();
        affFrom.union_(affTo);

        RDFVariable callFrom = affFrom.variable(subCall).property<nmo::from>();
        RDFVariable callFromAddress = callFrom.property<nco::hasIMAddress>(affAddress);

        RDFVariable callTo = affTo.variable(subCall).property<nmo::to>();
        RDFVariable callToAddress = callTo.property<nco::hasIMAddress>(affAddress);

//        affAddress.property<nco::hasIMAddress>(affIMAddress);

        /*
          (contact -> nco:hasAffiliation -> nco:hasPhoneNumber -> maemo:localPhoneNumber) ==
          (call nmo:from|nmo:to -> nco:hasPhoneNumber -> maemo:localPhoneNumber)
        */
        contacts[1].isOfType<nco::PersonContact>();
        RDFVariable affPhoneNumber = contacts[1].variable(LAT("phoneNumber"));
        RDFVariable affContactPhoneNumber = contacts[1].property<nco::hasAffiliation>()
            .property<nco::hasPhoneNumber>().property<maemo::localPhoneNumber>();

        RDFPattern affPhoneFrom = contacts[1].pattern().child();
        RDFPattern affPhoneTo = contacts[1].pattern().child();
        affPhoneFrom.union_(affPhoneTo);
        affPhoneFrom.variable(subCall).property<nmo::from>(affPhoneNumber);
        affPhoneTo.variable(subCall).property<nmo::to>(affPhoneNumber);

        affPhoneNumber.property<nco::hasPhoneNumber>()
            .property<maemo::localPhoneNumber>(affContactPhoneNumber);


        subSelect.addColumnAs(contactSelect.asExpression(), contact);

        RDFVariable idFilter = contact.filter(LAT("tracker:id"));
        query.addColumn(LAT("contactId"), idFilter);
        query.addColumn(LAT("contactFirstName"), contact.function<nco::nameGiven>());
        query.addColumn(LAT("contactLastName"), contact.function<nco::nameFamily>());
        query.addColumn(LAT("imNickname"), affAddress.function<nco::imNickname>());
    }

    if (propertyMask.contains(Event::Direction))
        query.addColumn(LAT("isSent"), outerCall.function<nmo::isSent>());
    if (propertyMask.contains(Event::IsMissedCall))
        query.addColumn(LAT("isAnswered"), outerCall.function<nmo::isAnswered>());
    if (propertyMask.contains(Event::IsEmergencyCall))
        query.addColumn(LAT("isEmergency"), outerCall.function<nmo::isEmergency>());
    if (propertyMask.contains(Event::IsRead))
        query.addColumn(LAT("isRead"), outerCall.function<nmo::isRead>());
    if (propertyMask.contains(Event::StartTime))
        query.addColumn(LAT("sentDate"), outerCall.function<nmo::sentDate>());
    if (propertyMask.contains(Event::EndTime))
        query.addColumn(LAT("receivedDate"), outerCall.function<nmo::receivedDate>());
    if (propertyMask.contains(Event::LastModified))
        query.addColumn(LAT("lastModified"), outerCall.function<nie::contentLastModified>());

    query.orderBy(date, false);

    callQuery = query;
}

QString TrackerIO::prepareMessagePartQuery(const QString &messageUri)
{
    QString query(LAT(
            "SELECT ?message "
              "?part "
              "?contentId "
              "nie:plainTextContent(?part) "
              "nie:mimeType(?part) "
              "nie:characterSet(?part) "
              "nie:contentSize(?part) "
              "nie:url(?part) "
            "WHERE { "
              "?message  nmo:mmsHasContent [nie:hasPart ?part] . "
              "?part nmo:contentId ?contentId "
            "FILTER(?message = <%1>)} "
            "ORDER BY ?contentId"));

    return query.arg(messageUri);
}

QString TrackerIO::prepareGroupQuery(const QString &localUid,
                                     const QString &remoteUid,
                                     int groupId)
{
    QString queryFormat(GROUP_QUERY);
    QStringList constrains;
    if (!remoteUid.isEmpty()) {
        QString number = normalizePhoneNumber(remoteUid);
        if (number.isEmpty()) {
            constrains << QString(LAT("?_channel nmo:hasParticipant [nco:hasIMAddress [nco:imID \"%1\"]] ."))
                          .arg(remoteUid);
        } else {
            constrains << QString(LAT("?_channel nmo:hasParticipant [nco:hasPhoneNumber [maemo:localPhoneNumber \"%1\"]] ."))
                          .arg(number.right(CommHistory::phoneNumberMatchLength()));
        }
    }
    if (!localUid.isEmpty()) {
        constrains << QString(LAT("FILTER(?_subject = \"%1\") ")).arg(localUid);
    }
    if (groupId != -1)
        constrains << QString(LAT("FILTER(?_channel = <%1>) ")).arg(Group::idToUrl(groupId).toString());

    return queryFormat.arg(constrains.join(LAT(" ")));
}

QUrl TrackerIOPrivate::uriForIMAddress(const QString &account, const QString &remoteUid)
{
    return QUrl(QString(LAT("telepathy:")) + account + QLatin1Char('!') + remoteUid);
}

QString TrackerIOPrivate::findLocalContact(UpdateQuery &query,
                                           const QString &accountPath)
{
    QString contact;
    QUrl uri = QString(LAT("telepathy:%1")).arg(accountPath);
    if (m_contactCache.contains(uri)) {
        contact = m_contactCache[uri];
    } else {
        contact = QString(LAT("[rdf:type nco:Contact; nco:hasIMAddress <%2>]"))
                  .arg(uri.toString());
        query.insertionRaw(uri,
                           "rdf:type",
                           LAT("nco:IMAddress"));

        m_contactCache.insert(uri, contact);
    }

    return contact;
}

QString TrackerIOPrivate::findIMContact(UpdateQuery &query,
                                        const QString &accountPath,
                                        const QString &imID)
{
    QString contact;
    QUrl imAddressURI = uriForIMAddress(accountPath, imID);

    if (m_contactCache.contains(imAddressURI)) {
        contact = m_contactCache[imAddressURI];
    } else {
        contact = QString(LAT("[rdf:type nco:Contact; nco:hasIMAddress <%2>]"))
                  .arg(imAddressURI.toString());

        query.insertionRaw(imAddressURI,
                           "rdf:type",
                           LAT("nco:IMAddress"));
        query.insertion(imAddressURI,
                        "nco:imID",
                        imID);

        m_contactCache.insert(imAddressURI, contact);
    }

    return contact;
}

QString TrackerIOPrivate::findPhoneContact(UpdateQuery &query,
                                           const QString &remoteId)
{
    QString contact;
    QUrl phoneNumberURI;

    QString phoneNumber = normalizePhoneNumber(remoteId);
    if (phoneNumber.isEmpty()) {
        phoneNumberURI = QString(LAT("tel:%1")).arg(remoteId);
    } else {
        phoneNumberURI = QString(LAT("tel:%1")).arg(phoneNumber);
    }

    if (m_contactCache.contains(phoneNumberURI)) {
        contact = m_contactCache[phoneNumberURI];
    } else {
        contact = QString(LAT("[rdf:type nco:Contact; nco:hasPhoneNumber <%2>]"))
                  .arg(phoneNumberURI.toString());

        query.insertionSilent(QString(LAT(
                "<%1> rdf:type nco:PhoneNumber; nco:phoneNumber \"%2\"; maemo:localPhoneNumber \"%3\" . "))
                        .arg(phoneNumberURI.toString())
                        .arg(phoneNumber)
                        .arg(phoneNumber.right(CommHistory::phoneNumberMatchLength())));

        m_contactCache.insert(phoneNumberURI, contact);
    }

    return contact;
}

QString TrackerIOPrivate::findRemoteContact(UpdateQuery &query,
                                            const QString &localUid,
                                            const QString &remoteUid)
{
    QString phoneNumber = normalizePhoneNumber(remoteUid);
    if (phoneNumber.isEmpty()) {
        return findIMContact(query, localUid, remoteUid);
    } else {
        return findPhoneContact(query, phoneNumber);
    }
}

void TrackerIOPrivate::writeCommonProperties(UpdateQuery &query,
                                             Event &event,
                                             bool modifyMode)
{
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
        case Event::BytesSent:
            // TODO: not implemented, do we need this?
            break;
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
                        "DELETE  {<%1> nmo:hasAttachment ?_part; nmo:mmsHasContent> ?_content ."
                        "?_content nie:hasPart ?_part ."
                        "?_part rdf:type rdf:Resource ."
                        "} WHERE {?_content nie:hasPart ?_part }")).arg(event.url().toString()));
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
                QString ccContact = findRemoteContact(query, event.localUid(), contactString);
                query.insertionRaw(event.url(),
                                   "nmo:cc",
                                   ccContact,
                                   false);
            }
            break;
        case Event::Bcc:
            if (modifyMode)
                query.deletion(event.url(),
                               "nmo:bcc");

            foreach (QString contactString, event.bccList()) {
                QString bccContact = findRemoteContact(query, event.localUid(), contactString);
                query.insertionRaw(event.url(),
                                   "nmo:bcc",
                                   bccContact,
                                   false);
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
                        "nie:url",
                        messagePart.contentLocation());
        query.insertion(part,
                        "nfo:fileName",
                        QFileInfo(messagePart.contentLocation()).fileName());

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

    QString remoteContact = findIMContact(query, event.localUid(), event.remoteUid());
    QString localContact = findLocalContact(query, event.localUid());

    if (event.direction() == Event::Outbound) {
        query.insertionRaw(eventSubject,
                           "nmo:from",
                           localContact);
        query.insertionRaw(eventSubject,
                           "nmo:to",
                           remoteContact);
    } else {
        query.insertionRaw(eventSubject,
                           "nmo:to",
                           localContact);
        query.insertionRaw(eventSubject,
                           "nmo:from",
                           remoteContact);
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

    QString remoteContact = findRemoteContact(query, event.localUid(), event.remoteUid());
    QString localContact = findLocalContact(query, event.localUid());

    if (event.direction() == Event::Outbound) {
        query.insertionRaw(eventSubject,
                           "nmo:from",
                           localContact);
        query.insertionRaw(eventSubject,
                           "nmo:to",
                           remoteContact);
    } else {
        query.insertionRaw(eventSubject,
                           "nmo:to",
                           localContact);
        query.insertionRaw(eventSubject,
                           "nmo:from",
                           remoteContact);
    }
}

void TrackerIOPrivate::addCallEvent(UpdateQuery &query, Event &event)
{
    event.setId(m_IdSource.nextEventId());
    QUrl eventSubject(event.url());

    query.insertionRaw(eventSubject,
                       "rdf:type",
                       LAT("nmo:Call"));

    QString remoteContact = findRemoteContact(query, event.localUid(), event.remoteUid());
    QString localContact = findLocalContact(query, event.localUid());

    if (event.direction() == Event::Outbound) {
        query.insertionRaw(eventSubject,
                           "nmo:from",
                           localContact);
        query.insertionRaw(eventSubject,
                           "nmo:to",
                           remoteContact);
    } else {
        query.insertionRaw(eventSubject,
                           "nmo:to",
                           localContact);
        query.insertionRaw(eventSubject,
                           "nmo:from",
                           remoteContact);
    }

    writeCommonProperties(query, event, false);

    writeCallProperties(query, event, false);
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
        QSqlError ret;
        ret.setType(QSqlError::TransactionError);
        ret.setDatabaseText(LAT("Event type not implemented"));
        d->lastError = ret;
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
        QSqlError ret;
        ret.setType(QSqlError::TransactionError);
        ret.setDatabaseText(LAT("Local uid required for group"));
        d->lastError = ret;
        return false;
    }

    // keep id of in-memory groups
    if (!group.isValid()) {
        group.setId(nextGroupId());
    }

    group.setPermanent(true);
    qDebug() << __FUNCTION__ << group.url() << group.localUid() << group.remoteUids();

    QString channelSubject = group.url().toString();

    query.insertionRaw(channelSubject,
                       "rdf:type",
                       LAT("nmo:CommunicationChannel"));

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
        QString participant = d->findIMContact(query, group.localUid(), remoteUid);
        query.insertionRaw(channelSubject,
                           "nmo:hasParticipant",
                           participant);
        query.insertion(channelSubject,
                        "nie:generator",
                        remoteUid);
    } else {
        QString participant = d->findPhoneContact(query, phoneNumber);
        query.insertionRaw(channelSubject,
                           "nmo:hasParticipant",
                           participant);
        query.insertion(channelSubject,
                        "nie:generator",
                        phoneNumber);
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
        QSqlError error;
        error.setType(QSqlError::TransactionError);
        error.setDatabaseText(LAT("Event not found"));
        lastError = error;
        return false;
    }

    QSparqlResultRow row = events->current();

    result.fillEventFromModel2(event);
    qDebug() << Q_FUNC_INFO << event.toString();

    if (event.type() == Event::MMSEvent) {
        QString partQuery = q->prepareMessagePartQuery(event.url().toString());

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
        QString remoteContact = d->findRemoteContact(query, event.localUid(), event.remoteUid());

        if (event.direction() == Event::Outbound) {
            query.insertionRaw(event.url(),
                               "nmo:from",
                               localContact);
            query.insertionRaw(event.url(),
                               "nmo:to",
                               remoteContact);
        } else {
            query.insertionRaw(event.url(),
                               "nmo:to",
                               localContact);
            query.insertionRaw(event.url(),
                               "nmo:from",
                               remoteContact);
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

    return d->handleQuery(QSparqlQuery(query.query(),
                                       QSparqlQuery::InsertStatement));
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
        QString remoteContact = d->findRemoteContact(query, QString(), event.remoteUid());
        query.insertionRaw(event.url(),
                           "nmo:from",
                           remoteContact,
                           true);
    }

    return d->handleQuery(QSparqlQuery(query.query(),
                                       QSparqlQuery::InsertStatement));
}

bool TrackerIO::deleteEvent(Event &event, QThread *backgroundThread)
{
    qDebug() << Q_FUNC_INFO << event.id() << backgroundThread;

    if (event.type() == Event::MMSEvent) {
        if (d->isLastMmsEvent(event.messageToken())) {
            d->getMmsDeleter(backgroundThread).deleteMessage(event.messageToken());
        }
    }

    QSparqlQuery deleteQuery(LAT("DELETE {?:uri a rdfs:Resource}"),
                             QSparqlQuery::DeleteStatement);
    deleteQuery.bindValue(LAT("uri"), event.url());

    return d->handleQuery(deleteQuery);
}

bool TrackerIO::getGroup(int id, Group &group)
{
    Group groupToFill;

    QSparqlQuery query(prepareGroupQuery(QString(), QString(), id));
    QueryResult result;

    QScopedPointer<QSparqlResult> groups(d->connection().exec(query));

    if (!d->runBlockedQuery(groups.data())) // FIXIT
        return false;

    if (groups->size() == 0) {
        QSqlError error;
        error.setType(QSqlError::TransactionError);
        error.setDatabaseText(LAT("Group not found"));
        d->lastError = error;
        return false;
    }

    groups->first();
    QSparqlResultRow row = groups->current();

    result.result = groups.data();
    result.fillGroupFromModel2(groupToFill);
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
    RDFSelect query;

    RDFVariable message = RDFVariable::fromType<nmo::Message>();
    message.property<nmo::communicationChannel>(Group::idToUrl(groupId));
    message.property<nmo::isDeleted>(LiteralValue(false));
    query.addCountColumn("total", message);

    QScopedPointer<QSparqlResult> queryResult(d->connection().exec(QSparqlQuery(query.getQuery())));

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
    RDFUpdate update;
    RDFVariable msg = RDFVariable::fromType<nmo::Message>();

    msg.property<nmo::communicationChannel>(Group::idToUrl(groupId));
    update.addDeletion(msg, nmo::isRead::iri(), RDFVariable());
    update.addInsertion(msg, nmo::isRead::iri(), LiteralValue(true));
    //Need to update the contentModifiedTime as well so that NOS gets update with the updated time
    QDateTime currDateTime = QDateTime::currentDateTime();

    update.addDeletion(msg, nie::contentLastModified::iri(), RDFVariable());
    update.addInsertion(msg, nie::contentLastModified::iri(), LiteralValue(currDateTime));

    return d->handleQuery(QSparqlQuery(update.getQuery(),
                                       QSparqlQuery::InsertStatement));
}

void TrackerIO::transaction(bool syncOnCommit)
{
    Q_ASSERT(d->m_pTransaction == 0);

    d->syncOnCommit = syncOnCommit;
    d->m_pTransaction = new CommittingTransaction(this);
    d->m_messageTokenRefCount.clear(); // make sure that nothing is removed if not requested
}

CommittingTransaction* TrackerIO::commit(bool isBlocking)
{
    Q_ASSERT(d->m_pTransaction);

    CommittingTransaction *returnTransaction = 0;

    if (isBlocking) {
        foreach (QSparqlQuery query, d->m_pTransaction->d->queries()) {
            QScopedPointer<QSparqlResult> result(d->connection().exec(query));
            if (!d->runBlockedQuery(result.data())) {
                break;
            }
        }
        delete d->m_pTransaction;
    } else {
        if (!d->m_pTransaction->d->queries().isEmpty()) {
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
        t->deleteLater(); // straight delete causes crash or lockup?
        m_pendingTransactions.dequeue();
        t = 0;

        if (!m_pendingTransactions.isEmpty())
            t = m_pendingTransactions.head();
    }

    if (t && !t->isRunning()) {
        foreach (QSparqlQuery query, t->d->queries()) {
            QScopedPointer<QSparqlResult> result(connection().exec(query));
            if (checkPendingResult(result.data(), false)) {
                t->d->addResult(result.take());
            } else {
                qWarning() << Q_FUNC_INFO << "abort transaction" << t;
                emit t->finished();
                m_pendingTransactions.dequeue();
                delete t;
                t = 0;
            }
        }

        if (t) {
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

    RDFUpdate update;
    switch(eventType) {
        // TODO: handle other event types
        case Event::CallEvent:
            // avoid "jump to case label crosses initialization" error by making
            // call variable's scope smaller
            {
                RDFVariable call = RDFVariable::fromType<nmo::Call>();
                update.addDeletion(call, rdf::type::iri(), rdfs::Resource::iri());
            }
            break;
        default:
            qDebug() << __FUNCTION__ << "Unsupported type" << eventType;
            return false;
    }

    return d->handleQuery(QSparqlQuery(update.getQuery(),
                                       QSparqlQuery::DeleteStatement));
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
    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::MMSMessage>();
    message.property<nmo::messageId>(LiteralValue(messageToken));
    query.addCountColumn("total", message);

    QScopedPointer<QSparqlResult> queryResult(connection().exec(QSparqlQuery(query.getQuery())));

    if (!runBlockedQuery(queryResult.data())) // FIXIT
        return false;

    if (queryResult->first()) {
        QSparqlResultRow row = queryResult->current();
        if (!row.isEmpty())
            total = row.value(0).toInt();
    }

    return (total == 1);
}

QSqlError TrackerIO::lastError() const
{
    return d->lastError;
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
        QSqlError ret;
        ret.setType(QSqlError::TransactionError);
        ret.setDatabaseText(result->lastError().message());
        lastError = ret;
        if (destroyOnFinished)
            delete result;
    }

    return !result->hasError();
}

bool TrackerIOPrivate::handleQuery(const QSparqlQuery &query)
{
    bool result = true;

    if (m_pTransaction) {
        // add query
        m_pTransaction->d->addQuery(query);
    } else {
        QSparqlResult *sResult = connection().exec(query);
        result = runBlockedQuery(sResult);
        delete sResult;
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
