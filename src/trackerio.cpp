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

#include "commonutils.h"
#include "event.h"
#include "group.h"
#include "messagepart.h"
#include "mmscontentdeleter.h"
#include "updatequery.h"
#include "queryresult.h"

#include "trackerio_p.h"
#include "trackerio.h"

using namespace CommHistory;
using namespace SopranoLive;

#define LAT(STR) QLatin1String(STR)

TrackerIOPrivate::TrackerIOPrivate(TrackerIO *parent)
    : q(parent),
    m_transaction(0),
    m_service(::tracker()),
    m_MmsContentDeleter(0),
    m_bgThread(0)
{
}

TrackerIOPrivate::~TrackerIOPrivate()
{
    if (m_MmsContentDeleter) {
        m_MmsContentDeleter->deleteLater();
        m_MmsContentDeleter = 0;
    }
}

TrackerIO::TrackerIO(QObject *parent)
    : QObject(parent),
    d(new TrackerIOPrivate(this))
{
}

TrackerIO::~TrackerIO()
{
    delete d;
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
    date = subMessage.property<nmo::sentDate>();
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
        RDFVariableList contacts = subContact.unionChildren(3);

        // by IM address...
        contacts[0].isOfType<nco::PersonContact>();
        RDFVariable imChannel = contacts[0].variable(channel);
        RDFVariable imParticipant;
        if (communicationChannel.isEmpty()) {
            imChannel = subMessage.property<nmo::communicationChannel>();
            imParticipant = imChannel.property<nmo::hasParticipant>();
        } else {
            imChannel = communicationChannel;
            imParticipant = imChannel.property<nmo::hasParticipant>();
        }
        RDFVariable imAddress = contacts[0].property<nco::hasIMAddress>();
        imAddress = imParticipant.property<nco::hasIMAddress>(); // not an assignment

        // or phone number (last digits)
        contacts[1].isOfType<nco::PersonContact>();
        RDFVariable phoneChannel = contacts[1].variable(channel);
        RDFVariable phoneParticipant;
        if (communicationChannel.isEmpty()) {
            phoneChannel = subMessage.property<nmo::communicationChannel>();
            phoneParticipant = phoneChannel.property<nmo::hasParticipant>();
        } else {
            phoneChannel = communicationChannel;
            phoneParticipant = phoneChannel.property<nmo::hasParticipant>();
        }
        RDFVariable number = phoneParticipant.property<nco::hasPhoneNumber>()
            .property<maemo::localPhoneNumber>();
        contacts[1].property<nco::hasPhoneNumber>().property<maemo::localPhoneNumber>() = number;

        // affiliation (work number)
        contacts[2].isOfType<nco::PersonContact>();
        RDFVariable affChannel = contacts[2].variable(channel);
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
        contacts[2].property<nco::hasAffiliation>()
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

        query.addColumn(LAT("contactId"), contact.function<nco::contactLocalUID>());
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
    date = subMessage.property<nmo::sentDate>();
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
        subContact.property<nco::hasIMAddress>(targetAddress);

        subSelect.addColumnAs(contactSelect.asExpression(), contact);

        query.addColumn(LAT("contactId"), contact.function<nco::contactLocalUID>());
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
        RDFVariableList contacts = subContact.unionChildren(4);

        /*
          contact -> nco:hasIMAddress) == (call from/to -> nco:hasIMAddress)
        */
        contacts[0].isOfType<nco::PersonContact>();
        RDFVariable imCall = contacts[0].variable(subCall);
        RDFVariable imAddress = contacts[0].variable(LAT("imAddress"));
        RDFVariable contactIMAddress = contacts[0].property<nco::hasIMAddress>();

        RDFPattern imFrom = contacts[0].pattern().child();
        RDFPattern imTo = contacts[0].pattern().child();
        imFrom.union_(imTo);
        imFrom.variable(subCall).property<nmo::from>(imAddress);
        imTo.variable(subCall).property<nmo::to>(imAddress);

        imAddress.property<nco::hasIMAddress>(contactIMAddress);

        /*
          work address:
          (contact -> nco:hasAffiliation -> nco:hasIMAddress) ==
          (call nmo:from|nmo:to -> nco:hasIMAddress)
        */
        contacts[1].isOfType<nco::PersonContact>();
        RDFVariable affCall = contacts[1].variable(subCall);
        RDFVariable affAddress = contacts[1].variable(LAT("imAddress"));
        RDFVariable affIMAddress = contacts[1].property<nco::hasIMAddress>();
        RDFVariable affiliation;
        contacts[1].property<nco::hasAffiliation>(affiliation);
        affiliation.property<nco::hasIMAddress>(affAddress);

        RDFPattern affFrom = contacts[1].pattern().child();
        RDFPattern affTo = contacts[1].pattern().child();
        affFrom.union_(affTo);
        affFrom.variable(subCall).property<nmo::from>(affAddress);
        affTo.variable(subCall).property<nmo::to>(affAddress);

        affAddress.property<nco::hasIMAddress>(contactIMAddress);

        /*
          (contact -> nco:hasPhoneNumber -> maemo:localPhoneNumber) ==
          (call nmo:from|nmo:to -> nco:hasPhoneNumber -> maemo:localPhoneNumber)
        */
        contacts[2].isOfType<nco::PersonContact>();

        RDFVariable phoneNumber = contacts[2].variable(LAT("phoneNumber"));
        RDFVariable contactPhoneNumber = contacts[2].property<nco::hasPhoneNumber>()
            .property<maemo::localPhoneNumber>();

        RDFPattern phoneFrom = contacts[2].pattern().child();
        RDFPattern phoneTo = contacts[2].pattern().child();
        phoneFrom.union_(phoneTo);
        phoneFrom.variable(subCall).property<nmo::from>(phoneNumber);
        phoneTo.variable(subCall).property<nmo::to>(phoneNumber);

        phoneNumber.property<nco::hasPhoneNumber>()
            .property<maemo::localPhoneNumber>(contactPhoneNumber);

        /*
          work number:
          (contact -> nco:hasAffiliation -> nco:hasPhoneNumber -> maemo:localPhoneNumber) ==
          (call nmo:from|nmo:to -> nco:hasPhoneNumber -> maemo:localPhoneNumber)
        */
        contacts[3].isOfType<nco::PersonContact>();
        RDFVariable affPhoneNumber = contacts[3].variable(LAT("phoneNumber"));
        RDFVariable affContactPhoneNumber = contacts[3].property<nco::hasAffiliation>()
            .property<nco::hasPhoneNumber>().property<maemo::localPhoneNumber>();

        RDFPattern affPhoneFrom = contacts[3].pattern().child();
        RDFPattern affPhoneTo = contacts[3].pattern().child();
        affPhoneFrom.union_(affPhoneTo);
        affPhoneFrom.variable(subCall).property<nmo::from>(affPhoneNumber);
        affPhoneTo.variable(subCall).property<nmo::to>(affPhoneNumber);

        affPhoneNumber.property<nco::hasPhoneNumber>()
            .property<maemo::localPhoneNumber>(affContactPhoneNumber);


        subSelect.addColumnAs(contactSelect.asExpression(), contact);

        query.addColumn(LAT("contactId"), contact.function<nco::contactLocalUID>());
        query.addColumn(LAT("contactFirstName"), contact.function<nco::nameGiven>());
        query.addColumn(LAT("contactLastName"), contact.function<nco::nameFamily>());
        query.addColumn(LAT("imNickname"), contactIMAddress.function<nco::imNickname>());
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

void TrackerIO::prepareMessagePartQuery(RDFSelect &query, RDFVariable &message)
{
    RDFVariable content = message.property<nmo::mmsHasContent>();
    RDFVariable part = content.property<nie::hasPart>();
    query.addColumn(LAT("message"), message);
    query.addColumn(LAT("part"), part);
    query.addColumn(LAT("contentId"), part.function<nmo::contentId>());
    query.addColumn(LAT("textContent"), part.function<nie::plainTextContent>());
    query.addColumn(LAT("contentType"), part.function<nie::mimeType>());
    query.addColumn(LAT("characterSet"), part.function<nie::characterSet>());
    query.addColumn(LAT("contentSize"), part.function<nie::contentSize>());
    query.addColumn(LAT("contentLocation"), part.function<nie::url>());
    query.orderBy(part.function<nmo::contentId>(), RDFSelect::Ascending);
}

void TrackerIO::prepareGroupQuery(RDFSelect &channelQuery,
                                  const QString &localUid,
                                  const QString &remoteUid,
                                  int groupId)
{
    RDFVariable channel = channelQuery.newColumn(LAT("channel"));
    RDFVariable subject = channelQuery.newColumn(LAT("subject")); // local tp account
    channelQuery.addColumn(LAT("remoteId"), channel.function<nie::generator>());

    // MUC title (topic/subject)
    channelQuery.addColumn(LAT("title"), channel.function<nie::title>());
    // CommHistory::Group::ChatType
    channelQuery.addColumn(LAT("identifier"), channel.function<nie::identifier>());

    RDFVariable contact = channelQuery.variable(LAT("contact"));
    RDFVariable imNickname = channelQuery.variable(LAT("imNickname"));

    RDFVariable lastDate = channelQuery.newColumn(LAT("lastDate"));
    RDFVariable lastMessage = channelQuery.newColumn(LAT("lastMessage"));
    RDFVariable lastModified = channelQuery.newColumn(LAT("lastModified"));
    channelQuery.addColumn(LAT("lastMessageText"), lastMessage.function<nie::plainTextContent>());
    channelQuery.addColumn(LAT("lastMessageSubject"), lastMessage.function<nmo::messageSubject>());

    RDFVariable vcard = lastMessage.function<nmo::fromVCard>();
    channelQuery.addColumn(LAT("lastVCardFileName"), vcard.function<nfo::fileName>());
    channelQuery.addColumn(LAT("lastVCardLabel"), vcard.function<rdfs::label>());
    channelQuery.addColumn(LAT("type"), lastMessage.function<rdf::type>());
    channelQuery.addColumn(LAT("deliveryStatus"), lastMessage.function<nmo::deliveryStatus>());

    {
        // select count of all messages and add it as an expression column to outer
        RDFSubSelect allSubsel;
        RDFVariable message = allSubsel.newCountColumn(LAT("total messages"));
        message.property<nmo::communicationChannel>(channel);
        message.property<nmo::isDeleted>(LiteralValue(false));
        channelQuery.addColumn(LAT("totalMessages"), allSubsel.asExpression());
    }

    {
        // select count of all already read messages and add it as an expression column to outer
        RDFSubSelect readSubsel;
        RDFVariable readMsg = readSubsel.newCountColumn(LAT("total unread messages"));
        readMsg.property<nmo::communicationChannel>(channel);
        readMsg.property<nmo::isRead>(LiteralValue(false));
        readMsg.property<nmo::isDeleted>(LiteralValue(false));
        channelQuery.addColumn(LAT("unreadMessages"), readSubsel.asExpression());
    }

    {
        // select count of all sent messages and add it as an expression column to outer
        RDFSubSelect sentSubsel;
        RDFVariable sentMsg = sentSubsel.newCountColumn(LAT("total sent messages"));
        sentMsg.property<nmo::communicationChannel>(channel);
        sentMsg.property<nmo::isSent>(LiteralValue(true));
        sentMsg.property<nmo::isDeleted>(LiteralValue(false));
        channelQuery.addColumn(LAT("sentMessages"), sentSubsel.asExpression());
    }

    RDFSubSelect innerSubsel;
    RDFVariable innerChannel = innerSubsel.newColumnAs<nmo::CommunicationChannel>(channel);
    RDFVariable innerSubject = innerSubsel.newColumnAs(subject);
    RDFVariable innerLastDate = innerSubsel.newColumnAs(lastDate);
    RDFVariable innerLastModified = innerSubsel.newColumnAs(lastModified);
    innerChannel.property<nie::subject>(innerSubject);
    innerChannel.property<nmo::lastMessageDate>(innerLastDate);
    innerChannel.property<nie::contentLastModified>(innerLastModified);

    // restrict by account / remote id
    if (!localUid.isEmpty()) {
        innerSubject == LiteralValue(localUid);
        if (!remoteUid.isEmpty()) {
            QString number = normalizePhoneNumber(remoteUid);
            if (number.isEmpty()) {
                innerChannel.property<nmo::hasParticipant>()
                    .property<nco::hasIMAddress>()
                    .property<nco::imID>(LiteralValue(remoteUid));
            } else {
                innerChannel.property<nmo::hasParticipant>()
                    .property<nco::hasPhoneNumber>()
                    .property<maemo::localPhoneNumber>() =
                    LiteralValue(number.right(CommHistory::phoneNumberMatchLength()));
            }
        }
    }

    if (groupId != -1)
        innerChannel == Group::idToUrl(groupId);

    {
        RDFSubSelect lastMsgSubsel;
        RDFVariable message = lastMsgSubsel.newColumn(LAT("message"));
        message.property<nmo::communicationChannel>(innerChannel);
        message.property<nmo::isDeleted>(LiteralValue(false));
        lastMsgSubsel
            .orderBy(message.property<nmo::sentDate>(), RDFSelect::Descending)
            .limit(1);

        innerSubsel.addColumnAs(lastMsgSubsel.asExpression(), lastMessage);
    }

    {
        // contact match
        RDFSubSelect contactSubsel;
        RDFVariable subContact = contactSubsel.newColumn(LAT("contact"));
        RDFVariable contactChannel = contactSubsel.variable(innerChannel);
        RDFVariableList contacts = subContact.unionChildren(3);

        // by IM address
        contacts[0].isOfType<nco::PersonContact>();
        RDFVariable imChannel = contacts[0].variable(contactChannel);
        RDFVariable imParticipant = imChannel.property<nmo::hasParticipant>();
        RDFVariable imAffiliation = contacts[0].property<nco::hasAffiliation>();
        RDFVariable imAddress = imAffiliation.property<nco::hasIMAddress>();
        imAddress = imParticipant.property<nco::hasIMAddress>();

        // by phone number (last digits)
        contacts[1].isOfType<nco::PersonContact>();
        RDFVariable phoneChannel = contacts[1].variable(contactChannel);
        RDFVariable phoneParticipant = phoneChannel.property<nmo::hasParticipant>();
        RDFVariable number = contacts[1].property<nco::hasPhoneNumber>()
            .property<maemo::localPhoneNumber>();
        number = phoneParticipant.property<nco::hasPhoneNumber>().
            property<maemo::localPhoneNumber>();

        // affiliation (work number)
        contacts[2].isOfType<nco::PersonContact>();
        RDFVariable affChannel = contacts[2].variable(contactChannel);
        RDFVariable affParticipant = affChannel.property<nmo::hasParticipant>();
        RDFVariable affNumber = contacts[2].property<nco::hasAffiliation>()
            .property<nco::hasPhoneNumber>().property<maemo::localPhoneNumber>();
        affNumber = affParticipant.property<nco::hasPhoneNumber>().
            property<maemo::localPhoneNumber>();
        innerSubsel.addColumnAs(contactSubsel.asExpression(), contact);
    }

    {
        // nickname associated with IM address
        RDFSubSelect nickSubSelect;
        RDFVariable nickChannel = nickSubSelect.variable(innerChannel);
        RDFVariable participant = nickChannel.property<nmo::hasParticipant>();
        nickSubSelect.addColumn(participant.property<nco::hasIMAddress>()
                                .property<nco::imNickname>());
        innerSubsel.addColumnAs(nickSubSelect.asExpression(), imNickname);
    }

    channelQuery.addColumn(LAT("contactId"), contact.function<nco::contactLocalUID>());
    channelQuery.addColumn(LAT("contactFirstName"), contact.function<nco::nameGiven>());
    channelQuery.addColumn(LAT("contactLastName"), contact.function<nco::nameFamily>());
    channelQuery.addColumn(LAT("imNickname"), imNickname);

    channelQuery.orderBy(lastDate, RDFSelect::Descending);
    channel.metaEnableStrategyFlags(RDFStrategy::IdentityColumn);
}

QUrl TrackerIOPrivate::uriForIMAddress(const QString &account, const QString &remoteUid)
{
    return QUrl(QString(LAT("telepathy:")) + account + QLatin1Char('!') + remoteUid);
}

QUrl TrackerIOPrivate::findLocalContact(UpdateQuery &query,
                                        const QString &accountPath)
{
    QUrl contact;
    QUrl uri = QString(LAT("telepathy:%1")).arg(accountPath);
    if (m_imContactCache.contains(uri)) {
        contact = m_imContactCache[uri];
    } else {
        contact = m_service->createUniqueIri(LAT("contactLocal"));
        query.insertion(uri,
                        rdf::type::iri(),
                        nco::IMAddress::iri());
        query.insertion(contact,
                        rdf::type::iri(),
                        nco::Contact::iri());
        query.insertion(contact,
                        nco::hasIMAddress::iri(),
                        uri);
        m_imContactCache.insert(uri, contact);
    }

    return contact;
}

QUrl TrackerIOPrivate::findIMContact(UpdateQuery &query,
                                     const QString &accountPath,
                                     const QString &imID)
{
    QUrl contact;
    QUrl imAddressURI = uriForIMAddress(accountPath, imID);

    if (m_imContactCache.contains(imAddressURI)) {
        contact = m_imContactCache[imAddressURI];
    } else {
        contact = m_service->createUniqueIri(LAT("contactIM"));
        query.insertion(imAddressURI,
                        rdf::type::iri(),
                        nco::IMAddress::iri());
        query.insertion(imAddressURI,
                        nco::imID::iri(),
                        LiteralValue(imID));
        query.insertion(contact,
                        rdf::type::iri(),
                        nco::Contact::iri());
        query.insertion(contact,
                        nco::hasIMAddress::iri(),
                        imAddressURI);
        m_imContactCache.insert(imAddressURI, contact);
    }

    return contact;
}

QUrl TrackerIOPrivate::findPhoneContact(UpdateQuery &query,
                                        const QString &accountPath,
                                        const QString &remoteId)
{
    Q_UNUSED(accountPath);

    QUrl contact;
    QUrl phoneNumberURI;

    QString phoneNumber = normalizePhoneNumber(remoteId);
    if (phoneNumber.isEmpty()) {
        phoneNumberURI = QString(LAT("tel:%1")).arg(remoteId);
    } else {
        phoneNumberURI = QString(LAT("tel:%1")).arg(phoneNumber);
    }

    if (m_imContactCache.contains(phoneNumberURI)) {
        contact = m_imContactCache[phoneNumberURI];
    } else {
        contact = m_service->createUniqueIri(LAT("contactPhone"));
        query.insertion(phoneNumberURI,
                        rdf::type::iri(),
                        nco::PhoneNumber::iri());
        query.insertion(contact,
                        rdf::type::iri(),
                        nco::Contact::iri());
        query.insertion(contact,
                        nco::hasPhoneNumber::iri(),
                        phoneNumberURI);
        m_imContactCache.insert(phoneNumberURI, contact);

        // insert nco:phoneNumber only if it doesn't exist
        // TODO: use INSERT SILENT when available in libqttracker.
        query.endQuery();
        query.startQuery();
        RDFVariable phone(phoneNumberURI);
        RDFVariable ncoPhoneNumber = phone.optional().property<nco::phoneNumber>();
        // phone.variable(...) moves the FILTER out of the OPTIONAL
        RDFFilter doesntExist = phone.variable(ncoPhoneNumber).isBound().not_();
        query.insertion(phone,
                        nco::phoneNumber::iri(),
                        LiteralValue(phoneNumber));

        // and the same for maemo:localPhoneNumber (the short version).
        query.endQuery();
        query.startQuery();
        RDFVariable localPhone(phoneNumberURI);
        RDFVariable localPhoneNumber = localPhone.optional().property<maemo::localPhoneNumber>();
        RDFFilter localDoesntExist = localPhone.variable(localPhoneNumber).isBound().not_();
        query.insertion(localPhone,
                        maemo::localPhoneNumber::iri(),
                        LiteralValue(phoneNumber.right(CommHistory::phoneNumberMatchLength())));
        query.endQuery();
    }

    return contact;
}

QUrl TrackerIOPrivate::findRemoteContact(UpdateQuery &query,
                                         const QString &localUid,
                                         const QString &remoteUid)
{
    QString phoneNumber = normalizePhoneNumber(remoteUid);
    if (phoneNumber.isEmpty()) {
        return findIMContact(query, localUid, remoteUid);
    } else {
        return findPhoneContact(query, localUid, phoneNumber);
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
                                nmo::sentDate::iri(),
                                LiteralValue(event.startTime()),
                                modifyMode);
            break;
        case Event::EndTime:
            if (event.endTime().isValid())
                query.insertion(event.url(),
                                nmo::receivedDate::iri(),
                                LiteralValue(event.endTime()),
                                modifyMode);
            break;
        case Event::Direction:
            if (event.direction() != Event::UnknownDirection) {
                bool isSent = event.direction() == Event::Outbound;
                query.insertion(event.url(),
                                nmo::isSent::iri(),
                                LiteralValue(isSent),
                                modifyMode);
            }
            break;
        case Event::IsDraft:
            query.insertion(event.url(),
                            nmo::isDraft::iri(),
                            LiteralValue(event.isDraft()),
                            modifyMode);
            break;
        case Event::IsRead:
            query.insertion(event.url(),
                            nmo::isRead::iri(),
                            LiteralValue(event.isRead()),
                            modifyMode);
            break;
        case Event::Status: {
            QUrl status;
            if (event.status() == Event::SentStatus) {
                status = nmo::delivery_status_sent::iri();
            } else if (event.status() == Event::DeliveredStatus) {
                status = nmo::delivery_status_delivered::iri();
            } else if (event.status() == Event::TemporarilyFailedStatus) {
                status = nmo::delivery_status_temporarily_failed::iri();
            } else if (event.status() == Event::PermanentlyFailedStatus) {
                status = nmo::delivery_status_permanently_failed::iri();
            }
            if (!status.isEmpty())
                query.insertion(event.url(),
                                nmo::deliveryStatus::iri(),
                                status,
                                modifyMode);
            else if (modifyMode)
                query.deletion(event.url(),
                               nmo::deliveryStatus::iri());
            break;
        }
        case Event::ReadStatus: {
            QUrl readStatus;
            if (event.readStatus() == Event::ReadStatusRead) {
                readStatus = nmo::read_status_read::iri();
            } else if (event.readStatus() == Event::ReadStatusDeleted) {
                readStatus = nmo::read_status_deleted::iri();
            }
            if (!readStatus.isEmpty())
                query.insertion(event.url(),
                                nmo::reportReadStatus::iri(),
                                readStatus,
                                modifyMode);
            break;
        }
        case Event::BytesSent:
            // TODO: not implemented, do we need this?
            break;
        case Event::BytesReceived:
            query.insertion(event.url(),
                            nie::contentSize::iri(),
                            LiteralValue(event.bytesReceived()),
                            modifyMode);
            break;
        case Event::Subject:
            query.insertion(event.url(),
                            nmo::messageSubject::iri(),
                            LiteralValue(event.subject()),
                            modifyMode);
            break;
        case Event::FreeText:
            query.insertion(event.url(),
                            nie::plainTextContent::iri(),
                            LiteralValue(event.freeText()),
                            modifyMode);
            break;
        case Event::MessageToken:
            query.insertion(event.url(),
                            nmo::messageId::iri(),
                            LiteralValue(event.messageToken()),
                            modifyMode);
            break;
        case Event::MmsId:
            query.insertion(event.url(),
                            nmo::mmsId::iri(),
                            LiteralValue(event.mmsId()),
                            modifyMode);
            break;
        case Event::CharacterSet:
            query.insertion(event.url(),
                            nmo::characterSet::iri(),
                            LiteralValue(event.characterSet()),
                            modifyMode);
            break;
        case Event::Language:
            query.insertion(event.url(),
                            nmo::language::iri(),
                            LiteralValue(event.language()),
                            modifyMode);
            break;
        case Event::IsDeleted:
            query.insertion(event.url(),
                            nmo::isDeleted::iri(),
                            LiteralValue(event.isDeleted()),
                            modifyMode);
            break;
        case Event::ReportDelivery:
            query.insertion(event.url(),
                            nmo::reportDelivery::iri(),
                            LiteralValue(event.reportDelivery()),
                            modifyMode);
        case Event::ReportRead:
            query.insertion(event.url(),
                            nmo::sentWithReportRead::iri(),
                            LiteralValue(event.reportRead()),
                            modifyMode);
            break;
        case Event::ReportReadRequested:
            query.insertion(event.url(),
                            nmo::mustAnswerReportRead::iri(),
                            LiteralValue(event.reportReadRequested()),
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
                            nmo::phoneMessageId::iri(),
                            LiteralValue(event.parentId()),
                            modifyMode);
            break;
        case Event::Encoding:
            query.insertion(event.url(),
                            nmo::encoding::iri(),
                            LiteralValue(event.encoding()),
                            modifyMode);
            break;
        case Event::FromVCardFileName:
            if (!event.validProperties().contains(Event::FromVCardLabel)) {
                qWarning() << Q_FUNC_INFO << "VCardFileName without valid VCard label";
                continue;
            }

            // if there is no filename set for the vcard, then we don't save anything
            if (!event.fromVCardFileName().isEmpty()) {
                if (modifyMode) {
                    query.startQuery();
                    RDFVariable eventSubject(event.url());
                    // TODO: check insert works when deleting WHERE fail
                    RDFVariable oldVcard(QString(LAT("oldVcard")));

                    eventSubject.property<nmo::fromVCard>(oldVcard);
                    query.deletion(oldVcard,
                                   rdf::type::iri(),
                                   rdfs::Resource::iri());
                    query.deletion(eventSubject,
                                   nmo::fromVCard::iri(),
                                   oldVcard);
                    query.endQuery();
                }

                RDFVariable vcard(QString(LAT("vcard")));
                query.insertion(event.url(),
                                nmo::fromVCard::iri(),
                                vcard);
                query.insertion(vcard,
                                rdf::type::iri(),
                                nfo::FileDataObject::iri());
                query.insertion(vcard,
                                nfo::fileName::iri(),
                                LiteralValue(event.fromVCardFileName()));
                query.insertion(vcard,
                                rdfs::label::iri(),
                                LiteralValue(event.fromVCardLabel()));
            }
            break;
        case Event::ValidityPeriod:
            query.insertion(event.url(),
                            nmo::validityPeriod::iri(),
                            LiteralValue(event.validityPeriod()),
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
                RDFVariable part(QString(LAT("part")));
                RDFVariable content(QString(LAT("content")));
                RDFVariable eventNode = event.url();
                eventNode.property<nmo::mmsHasContent>(content);
                content.property<nmo::hasPart>(part);

                query.startQuery();
                query.deletion(part,
                               rdf::type::iri(),
                               rdf::Resource::iri());
                query.deletion(event.url(),
                               nmo::hasAttachment::iri(),
                               part);
                query.deletion(content,
                               nmo::hasPart::iri(),
                               part);
                query.endQuery();

                query.startQuery();
                // new variables to remove connection between "content" and "part"
                RDFVariable eventNode2 = event.url();
                RDFVariable eventContent;
                eventNode2.property<nmo::mmsHasContent>(eventContent);
                query.deletion(eventContent,
                               rdf::type::iri(),
                               rdf::Resource::iri());
                query.deletion(event.url(),
                               nmo::mmsHasContent::iri(),
                               eventContent);
                query.endQuery();
            }
            addMessageParts(query, event);
            break;
        case Event::To:
            if (modifyMode) {
                RDFVariable header(QString(LAT("mmsHeader")));
                RDFVariable eventSubject(event.url());

                eventSubject.property<nmo::messageHeader>(header);

                query.startQuery();
                query.deletion(header,
                               rdf::type::iri(),
                               rdfs::Resource::iri());
                query.deletion(eventSubject,
                               nmo::messageHeader::iri(),
                               header);
                query.endQuery();
            }

            // Store To list in message header x-mms-to
            if (!event.toList().isEmpty()) {
                RDFVariable header(QString(LAT("header")));
                query.insertion(header,
                                rdf::type::iri(),
                                nmo::MessageHeader::iri());
                query.insertion(header,
                                nmo::headerName::iri(),
                                LiteralValue(LAT("x-mms-to")));
                query.insertion(header,
                                nmo::headerValue::iri(),
                                LiteralValue(event.toList().join(LAT(";"))));
                query.insertion(event.url(),
                                nmo::messageHeader::iri(),
                                header);
            }
            break;
        case Event::Cc:
            if (modifyMode) {
                //TODO: clean contact nodes
                query.startQuery();
                query.deletion(event.url(),
                               nmo::cc::iri());
                query.endQuery();
            }
            foreach (QString contactString, event.ccList()) {
                QUrl ccContact = findRemoteContact(query, event.localUid(), contactString);
                query.insertion(event.url(),
                                nmo::cc::iri(),
                                ccContact,
                                false);
            }
            break;
        case Event::Bcc:
            if (modifyMode) {
                //TODO: clean contact nodes
                query.startQuery();
                query.deletion(event.url(),
                               nmo::bcc::iri());
                query.endQuery();
            }
            foreach (QString contactString, event.bccList()) {
                QUrl bccContact = findRemoteContact(query, event.localUid(), contactString);
                query.insertion(event.url(),
                                nmo::bcc::iri(),
                                bccContact,
                                false);
            }
            break;
        case Event::ContentLocation:
            query.insertion(event.url(),
                            nie::generator::iri(),
                            LiteralValue(event.contentLocation()),
                            modifyMode);
            break;
        default:; //do nothing
        }
    }
}

void TrackerIOPrivate::writeCallProperties(UpdateQuery &query, Event &event, bool modifyMode)
{
    query.insertion(event.url(),
                    nmo::isAnswered::iri(),
                    LiteralValue(!event.isMissedCall()),
                    modifyMode);
    query.insertion(event.url(),
                    nmo::isEmergency::iri(),
                    LiteralValue(event.isEmergencyCall()),
                    modifyMode);
    query.insertion(event.url(),
                    nmo::duration::iri(),
                    LiteralValue(event.endTime().toTime_t() - event.startTime().toTime_t()),
                    modifyMode);
}

void TrackerIOPrivate::addMessageParts(UpdateQuery &query, Event &event)
{
    RDFVariable content(QString(LAT("content")));
    RDFVariable eventSubject(event.url());

    query.insertion(content,
                    rdf::type::iri(),
                    nmo::Multipart::iri());
    query.insertion(eventSubject,
                    nmo::mmsHasContent::iri(),
                    content);

    foreach (const MessagePart &messagePart, event.messageParts()) {
        RDFVariable part(QUrl(m_service->createUniqueIri(LAT("mmsPart"))));
        query.insertion(part,
                        rdf::type::iri(),
                        nmo::Attachment::iri());

        // set nmo:MimePart properties
        query.insertion(part,
                        nmo::contentId::iri(),
                        LiteralValue(messagePart.contentId()));
        query.insertion(part,
                        nie::contentSize::iri(),
                        LiteralValue(messagePart.contentSize()));
        query.insertion(part,
                        nie::url::iri(),
                        LiteralValue(messagePart.contentLocation()));
        query.insertion(part,
                        nfo::fileName::iri(),
                        LiteralValue(QFileInfo(messagePart.contentLocation()).fileName()));

        // TODO: how exactly should we handle these?
        if (messagePart.contentType().startsWith(LAT("image/"))) {
            query.insertion(part,
                            rdf::type::iri(),
                            nfo::Image::iri());
        } else if (messagePart.contentType().startsWith(LAT("audio/"))) {
            query.insertion(part,
                            rdf::type::iri(),
                            nfo::Audio::iri());
        } else if (messagePart.contentType().startsWith(LAT("video/"))) {
            query.insertion(part,
                            rdf::type::iri(),
                            nfo::Video::iri());
        } else {
            query.insertion(part,
                            rdf::type::iri(),
                            nfo::PlainTextDocument::iri());
        }

        // set nie:InformationElement properties
        query.insertion(part,
                        nie::plainTextContent::iri(),
                        LiteralValue(messagePart.plainTextContent()));
        query.insertion(part,
                        nie::mimeType::iri(),
                        LiteralValue(messagePart.contentType()));
        query.insertion(part,
                        nie::characterSet::iri(),
                        LiteralValue(messagePart.characterSet()));
        query.insertion(content,
                        nmo::hasPart::iri(),
                        part);

        if (messagePart.contentType() != LAT("text/plain") &&
            messagePart.contentType() != LAT("application/smil"))
        {
            qDebug() << "[MMS-ATTACH] Adding attachment" << messagePart.contentLocation() << messagePart.contentType() << "to message" << event.url();
            query.insertion(eventSubject,
                            nmo::hasAttachment::iri(),
                            part);
        }
    }
}

void TrackerIOPrivate::addIMEvent(UpdateQuery &query, Event &event)
{
    event.setId(m_IdSource.nextEventId());
    RDFVariable eventSubject(event.url());

    query.insertion(eventSubject,
                    rdf::type::iri(),
                    nmo::IMMessage::iri());

    QUrl remoteContact = findIMContact(query, event.localUid(), event.remoteUid());
    QUrl localContact = findLocalContact(query, event.localUid());

    if (event.direction() == Event::Outbound) {
        query.insertion(eventSubject,
                        nmo::from::iri(),
                        localContact);
        query.insertion(eventSubject,
                        nmo::to::iri(),
                        remoteContact);
    } else {
        query.insertion(eventSubject,
                        nmo::to::iri(),
                        localContact);
        query.insertion(eventSubject,
                        nmo::from::iri(),
                        remoteContact);
    }
    writeCommonProperties(query, event, false);
}

void TrackerIOPrivate::addSMSEvent(UpdateQuery &query, Event &event)
{
    event.setId(m_IdSource.nextEventId());
    RDFVariable eventSubject(event.url());

    if (event.type() == Event::MMSEvent) {
        query.insertion(eventSubject,
                        rdf::type::iri(),
                        nmo::MMSMessage::iri());
    } else if (event.type() == Event::SMSEvent) {
        query.insertion(eventSubject,
                        rdf::type::iri(),
                        nmo::SMSMessage::iri());
    }

    //TODO: add check that group exist as part of the query
    writeCommonProperties(query, event, false);
    writeSMSProperties(query, event, false);
    if (event.type() == Event::MMSEvent)
        writeMMSProperties(query, event, false);

    QUrl remoteContact = findRemoteContact(query, event.localUid(), event.remoteUid());
    QUrl localContact = findLocalContact(query, event.localUid());

    if (event.direction() == Event::Outbound) {
        query.insertion(eventSubject,
                        nmo::from::iri(),
                        localContact);
        query.insertion(eventSubject,
                        nmo::to::iri(),
                        remoteContact);
    } else {
        query.insertion(eventSubject,
                        nmo::to::iri(),
                        localContact);
        query.insertion(eventSubject,
                        nmo::from::iri(),
                        remoteContact);
    }
}

void TrackerIOPrivate::addCallEvent(UpdateQuery &query, Event &event)
{
    event.setId(m_IdSource.nextEventId());
    RDFVariable eventSubject(event.url());

    query.insertion(eventSubject,
                    rdf::type::iri(),
                    nmo::Call::iri());

    QUrl remoteContact = findRemoteContact(query, event.localUid(), event.remoteUid());
    QUrl localContact = findLocalContact(query, event.localUid());

    if (event.direction() == Event::Outbound) {
        query.insertion(eventSubject,
                        nmo::from::iri(),
                        localContact);
        query.insertion(eventSubject,
                        nmo::to::iri(),
                        remoteContact);
    } else {
        query.insertion(eventSubject,
                        nmo::to::iri(),
                        localContact);
        query.insertion(eventSubject,
                        nmo::from::iri(),
                        remoteContact);
    }

    writeCommonProperties(query, event, false);

    writeCallProperties(query, event, false);
}

void TrackerIOPrivate::setChannel(UpdateQuery &query, Event &event, int channelId, bool modify)
{
    QUrl channelUrl = Group::idToUrl(channelId);

    query.insertion(channelUrl,
                    nmo::lastMessageDate::iri(),
                    LiteralValue(event.endTime()),
                    true);
    query.insertion(channelUrl,
                    nie::contentLastModified::iri(),
                    LiteralValue(event.endTime()),
                    true);
    query.insertion(event.url(),
                    nmo::communicationChannel::iri(),
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
            // do it in a separate query, cause the property may not exist yet
            query.startQuery();
            d->setFolderLastModifiedTime(query, event.parentId(), QDateTime::currentDateTime());
            query.endQuery();
        }

        // specify not-inherited classes only when adding events, not during modifications
        // to have tracker errors when modifying non-existent events
        query.insertion(event.url(),
                        rdf::type::iri(),
                        nie::DataObject::iri());

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
                    nie::contentLastModified::iri(),
                    LiteralValue(event.lastModified()));

    d->m_service->executeQuery(query.rdfUpdate());

    return true;
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

    RDFVariable channelSubject(group.url());

    query.insertion(channelSubject,
                    rdf::type::iri(),
                    nmo::CommunicationChannel::iri());

    // TODO: ontology
    query.insertion(channelSubject,
                    nmo::subject::iri(),
                    LiteralValue(group.localUid()));

    query.insertion(channelSubject,
                    nie::identifier::iri(),
                    LiteralValue(QString::number(group.chatType())));

    query.insertion(channelSubject,
                    nie::title::iri(),
                    LiteralValue(group.chatName()));

    QString remoteUid = group.remoteUids().first();
    QString phoneNumber = normalizePhoneNumber(remoteUid);

    if (phoneNumber.isEmpty()) {
        QUrl participant = d->findIMContact(query, group.localUid(), remoteUid);
        query.insertion(channelSubject,
                        nmo::hasParticipant::iri(),
                        participant);
        query.insertion(channelSubject,
                        nie::generator::iri(),
                        LiteralValue(remoteUid));
    } else {
        QUrl participant = d->findPhoneContact(query, group.localUid(), phoneNumber);
        query.insertion(channelSubject,
                        nmo::hasParticipant::iri(),
                        participant);
        query.insertion(channelSubject,
                        nie::generator::iri(),
                        LiteralValue(phoneNumber));
    }

    query.insertion(channelSubject,
                    nmo::lastMessageDate::iri(),
                    LiteralValue(QDateTime::fromTime_t(0)));

    group.setLastModified(QDateTime::currentDateTime());
    query.insertion(channelSubject,
                    nie::contentLastModified::iri(),
                    LiteralValue(group.lastModified()));

    d->m_service->executeQuery(query.rdfUpdate());

    return true;
}

template<class CopyOntology>
QStringList TrackerIOPrivate::queryMMSCopyAddresses(Event &event)
{
    LiveNodes model;
    RDFSelect   copyQuery;
    RDFVariable copyMessage = event.url();
    RDFVariable copyContact = copyMessage.property<CopyOntology>();

    RDFVariable mergedCopyAddresses;
    mergedCopyAddresses.unionMerge(RDFVariableList()
                                   << copyContact.optional().property<nco::hasPhoneNumber>()
                                   .property<nco::phoneNumber>()
                                   << copyContact.optional().property<nco::hasIMAddress>()
                                   .property<nco::imID>());
    copyQuery.addColumn(LAT("contact"), mergedCopyAddresses);
    model = ::tracker()->modelQuery(copyQuery);

    QStringList copyList;
    if (model->rowCount()) {
        for (int row = 0; row < model->rowCount(); row++) {
            copyList << model->index(row, 0).data().toString();
        }
    }
    qDebug()<<"[MMS-COMM] Extracted CC/BCC list::"<<copyList;
    return copyList;
}

QStringList TrackerIOPrivate::queryMmsToAddresses(Event &event)
{
    LiveNodes model;
    RDFSelect   query;
    RDFVariable message = event.url();
    RDFVariable header = message.property<nmo::messageHeader>();

    header.property<nmo::headerName>(LiteralValue("x-mms-to"));

    query.addColumn(LAT("header"), header.property<nmo::headerValue>());
    model = ::tracker()->modelQuery(query);

    QStringList copyList;

    if (model->rowCount() > 0 ) {
        copyList << model->index(0, 0).data().toString().split(";", QString::SkipEmptyParts);
    }

    qDebug()<<"[MMS-COMM] Extracted To list::" << copyList;
    return copyList;
}

// direct instanciation for specific ontolgies
template
QStringList TrackerIOPrivate::queryMMSCopyAddresses<nmo::cc>(Event &event);
template
QStringList TrackerIOPrivate::queryMMSCopyAddresses<nmo::bcc>(Event &event);

bool TrackerIOPrivate::querySingleEvent(RDFSelect query, Event &event)
{
    QueryResult result;
    LiveNodes model = ::tracker()->modelQuery(query);
    result.model = model;
    result.propertyMask = Event::allProperties();
    if (!model->rowCount()) {
        QSqlError error;
        error.setType(QSqlError::TransactionError);
        error.setDatabaseText(LAT("Event not found"));
        lastError = error;
        return false;
    }
    for (int i = 0; i < model->columnCount(); i++) {
        result.columns.insert(model->headerData(i, Qt::Horizontal).toString(), i);
    }
    QueryResult::fillEventFromModel(result, 0, event);

    if (event.type() == Event::MMSEvent) {
        RDFSelect partQuery;
        RDFVariable message = event.url();
        q->prepareMessagePartQuery(partQuery, message);
        model = ::tracker()->modelQuery(partQuery);
        result.model = model;

        if (model->rowCount()) {
            for (int i = 0; i < model->columnCount(); i++) {
                result.columns.insert(model->headerData(i, Qt::Horizontal).toString(), i);
            }
            for (int row = 0; row < model->rowCount(); row++) {
                MessagePart part;
                QueryResult::fillMessagePartFromModel(result, row, part);
                event.addMessagePart(part);
            }
        }

        QStringList copyAddresses;
        copyAddresses = queryMMSCopyAddresses<nmo::cc> (event);
        event.setCcList(copyAddresses);

        copyAddresses = queryMMSCopyAddresses<nmo::bcc> (event);
        event.setBccList(copyAddresses);

        event.setToList(queryMmsToAddresses(event));
        event.resetModifiedProperties();
    }

    return true;
}

bool TrackerIO::getEvent(int id, Event &event)
{
    RDFSelect query;

    RDFVariable msg;
    msg == Event::idToUrl(id);

    RDFVariable type = msg.type();
    type.isMemberOf(RDFVariableList() << nmo::Message::iri()
                    << nmo::Call::iri());
    query.addColumn(LAT("type"), type);

    LiveNodes model = ::tracker()->modelQuery(query);
    int count = model->rowCount();
    bool isCall = (count == 2);

    if (count == 0) {
        QSqlError error;
        error.setType(QSqlError::TransactionError);
        error.setDatabaseText(LAT("Event not found"));
        d->lastError = error;
        return false;
    }

    RDFVariable message = RDFVariable::fromType<nmo::Message>();
    message == Event::idToUrl(id);
    if (isCall) {
        prepareCallQuery(query, message, Event::allProperties());
    } else {
        prepareMessageQuery(query, message, Event::allProperties());
    }

    return d->querySingleEvent(query, event);
}

bool TrackerIO::getEventByMessageToken(const QString& token, Event &event)
{
    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::Message>();
    message.property<nmo::messageId>() = LiteralValue(token);
    prepareMessageQuery(query, message, Event::allProperties());

    return d->querySingleEvent(query, event);
}

bool TrackerIO::getEventByMessageToken(const QString &token, int groupId, Event &event)
{
    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::Message>();
    message.property<nmo::messageId>() = LiteralValue(token);
    message.property<nmo::communicationChannel>(Group::idToUrl(groupId));
    prepareMessageQuery(query, message, Event::allProperties());

    return d->querySingleEvent(query, event);
}

bool TrackerIO::getEventByMmsId(const QString& mmsId, int groupId, Event &event)
{
    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::Message>();
    message.property<nmo::mmsId>() = LiteralValue(mmsId);

    //when sending to self number, the id of the message will be the same, but we need to pick outgoing message here
    message.property<nmo::isSent>(LiteralValue(true));
    message.property<nmo::communicationChannel>(Group::idToUrl(groupId));

    prepareMessageQuery(query, message, Event::allProperties());

    return d->querySingleEvent(query, event);
}

bool TrackerIO::getEventByUri(const QUrl& uri, Event &event)
{
    int eventId = Event::urlToId(uri.toString());
    return getEvent(eventId,event);
}

bool TrackerIO::modifyEvent(Event &event)
{
    UpdateQuery query;

    event.setLastModified(QDateTime::currentDateTime()); //always update modified times in case of modifyEvent
                                                                                                         //irrespective whether client sets or not
    // allow uid changes for drafts
    if (event.isDraft()
        && (event.validProperties().contains(Event::LocalUid)
            || event.validProperties().contains(Event::RemoteUid))) {
        // TODO: allow multiple remote uids
        QUrl localContact = d->findLocalContact(query, event.localUid());
        QUrl remoteContact = d->findRemoteContact(query, event.localUid(), event.remoteUid());

        if (event.direction() == Event::Outbound) {
            query.insertion(event.url(),
                            nmo::from::iri(),
                            localContact);
            query.insertion(event.url(),
                            nmo::to::iri(),
                            remoteContact);
        } else {
            query.insertion(event.url(),
                            nmo::to::iri(),
                            localContact);
            query.insertion(event.url(),
                            nmo::from::iri(),
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
                        nie::contentLastModified::iri(),
                        LiteralValue(event.lastModified()),
                        true);
    }

    if (event.type() == Event::CallEvent)
        d->writeCallProperties(query, event, true);

    d->writeCommonProperties(query, event, true);

    d->m_service->executeQuery(query.rdfUpdate());

    return true;
}

bool TrackerIO::modifyGroup(Group &group)
{
    UpdateQuery query;

    Group::PropertySet propertySet = group.modifiedProperties();
    foreach (Group::Property property, propertySet) {
        switch (property) {
            case Group::ChatName:
                query.insertion(group.url(),
                                nie::title::iri(),
                                LiteralValue(group.chatName()),
                                true);
                break;
            case Group::EndTime:
                if (group.endTime().isValid())
                    query.insertion(group.url(),
                                    nmo::lastMessageDate::iri(),
                                    LiteralValue(group.endTime()),
                                    true);
                break;
            case Group::LastModified:
                if ( group.lastModified() > QDateTime::fromTime_t(0) )
                    query.insertion(group.url(),
                                    nie::contentLastModified::iri(),
                                    LiteralValue(group.lastModified()),
                                    true);
            default:; // do nothing
        }
    }

    d->m_service->executeQuery(query.rdfUpdate());

    return true;
}

bool TrackerIO::moveEvent(Event &event, int groupId)
{
    qDebug() << Q_FUNC_INFO << event.id() << groupId;
    UpdateQuery query;

    d->setChannel(query, event, groupId, true); // true means modify

    if (event.direction() == Event::Inbound) {
        qDebug() << Q_FUNC_INFO << "Direction INBOUND";
        QUrl remoteContact = d->findRemoteContact(query, QString(), event.remoteUid());
        query.insertion(event.url(),
                        nmo::from::iri(),
                        remoteContact,
                        true); //TODO: proper contact deletion
    }

    d->m_service->executeQuery(query.rdfUpdate());

    return true;
}

bool TrackerIO::deleteEvent(Event &event, QThread *backgroundThread)
{
    qDebug() << Q_FUNC_INFO << event.id() << backgroundThread;

    Live<nmo::Message> msg = d->m_service->liveNode(event.url());

    if (event.type() == Event::MMSEvent) {
        if (d->isLastMmsEvent(event.messageToken())) {
            d->getMmsDeleter(backgroundThread).deleteMessage(event.messageToken());
        }
    }

    msg->remove();

    return true;
}

bool TrackerIO::getGroup(int id, Group &group)
{
    Group groupToFill;

    RDFSelect query;
    prepareGroupQuery(query, QString(), QString(), id);
    QueryResult result;
    result.query = query;
    result.model = ::tracker()->modelQuery(query);

    if (result.model->rowCount() == 0) {
        QSqlError error;
        error.setType(QSqlError::TransactionError);
        error.setDatabaseText(LAT("Group not found"));
        d->lastError = error;
        return false;
    }

    for (int i = 0; i < result.model->columnCount(); i++)
        result.columns.insert(result.model->headerData(i, Qt::Horizontal).toString(), i);
    QueryResult::fillGroupFromModel(result, 0, groupToFill);
    group = groupToFill;

    return true;
}

void TrackerIOPrivate::getMmsListForDeletingByGroup(int groupId, LiveNodes& model)
{
    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::MMSMessage>();
    message.property<nmo::communicationChannel>(Group::idToUrl(groupId));
    query.addColumn(message);
    query.addColumn("messageId", message.function<nmo::messageId>());

    RDFSubSelect msgCountQuery;
    RDFVariable innerMessage = RDFVariable::fromType<nmo::MMSMessage>();
    innerMessage.property<nmo::isDeleted>(LiteralValue(false));
    innerMessage.property<nmo::messageId>(msgCountQuery.variable(message).property<nmo::messageId>());
    msgCountQuery.addCountColumn("total", innerMessage);

    query.addColumn(msgCountQuery.asExpression());

    model = ::tracker()->modelQuery(query);
}

void TrackerIOPrivate::deleteMmsContentByGroup(int groupId)
{
    // delete mms messages content from fs
    LiveNodes model;
    getMmsListForDeletingByGroup(groupId, model);
    for(int r = 0; r < model->rowCount(); ++r){
        QString messageToken(model->index(r,1).data().toString());

        if (!messageToken.isEmpty()) {
            int refCount(model->index(r,2).data().toInt());

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
}

bool TrackerIO::deleteGroup(int groupId, bool deleteMessages, QThread *backgroundThread)
{
    Q_UNUSED(backgroundThread);
    qDebug() << __FUNCTION__ << groupId << deleteMessages << backgroundThread;

    d->m_bgThread = backgroundThread;

    // error return left for possible future implementation

    QUrl group = Group::idToUrl(groupId);
    RDFUpdate update;

    if (deleteMessages) {
        RDFVariable msg = RDFVariable::fromType<nmo::Message>();
        msg.property<nmo::communicationChannel>(group);
        update.addDeletion(msg, rdf::type::iri(), rdfs::Resource::iri());

        // delete mms attachments
        d->deleteMmsContentByGroup(groupId);
    }

    // delete conversation
    update.addDeletion(group, rdf::type::iri(), rdfs::Resource::iri());

    d->m_service->executeQuery(update);

    return true;
}

bool TrackerIO::totalEventsInGroup(int groupId, int &totalEvents)
{
    totalEvents = -1;
    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::Message>();
    message.property<nmo::communicationChannel>(Group::idToUrl(groupId));
    message.property<nmo::isDeleted>(LiteralValue(false));
    query.addCountColumn("total", message);
    LiveNodes model = ::tracker()->modelQuery(query);
    if (model->rowCount() > 0) {
        totalEvents = model->index(0, 0).data().toInt();
    }

    return true;
}

bool TrackerIO::markAsReadGroup(int groupId)
{
    qDebug() << Q_FUNC_INFO << groupId;

    RDFUpdate update;
    RDFVariable msg = RDFVariable::fromType<nmo::Message>();

    msg.property<nmo::communicationChannel>(Group::idToUrl(groupId));
    update.addDeletion(msg, nmo::isRead::iri(), RDFVariable());
    update.addInsertion(msg, nmo::isRead::iri(), LiteralValue(true));
    //Need to update the contentModifiedTime as well so that NOS gets update with the updated time
    QDateTime currDateTime = QDateTime::currentDateTime();
    qDebug() << "Setting modified time for group" << currDateTime;
    update.addDeletion(msg, nie::contentLastModified::iri(), RDFVariable());
    update.addInsertion(msg, nie::contentLastModified::iri(), LiteralValue(currDateTime));

    d->m_service->executeQuery(update);

    return true;
}

void TrackerIO::transaction(bool syncOnCommit)
{
    RDFTransaction::Mode mode = syncOnCommit ?
        (RDFTransaction::Mode)BackEnds::Tracker::SyncOnCommit :
        (RDFTransaction::Mode)RDFTransaction::Default;

    d->m_transaction = ::tracker()->createTransaction(mode);
    if (d->m_transaction) {
        d->m_service = d->m_transaction->service();
    } else {
        qWarning() << __PRETTY_FUNCTION__ << ": error starting transaction";
        d->m_service = ::tracker();
    }

    d->m_messageTokenRefCount.clear(); // make sure that nothing is removed if not requested
}

QSharedPointer<SopranoLive::RDFTransaction> TrackerIO::commit(bool isBlocking)
{
    RDFTransactionPtr result = d->m_transaction;

    if (d->m_transaction) {
        d->m_transaction->commit(isBlocking);
    }
    d->m_service = ::tracker();
    d->m_transaction.clear();
    d->m_imContactCache.clear();

    d->checkAndDeletePendingMmsContent(d->m_bgThread);

    return result;
}

void TrackerIO::rollback()
{
    if (d->m_transaction) {
        d->m_transaction->rollback();
    }
    d->m_service = ::tracker();
    d->m_transaction.clear();
    d->m_imContactCache.clear();
    d->m_messageTokenRefCount.clear(); // Clear cache to avoid deletion after rollback
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

    d->m_service->executeQuery(update);
    return true;
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
        folder = nmo::predefined_phone_msg_folder_inbox::iri();
    } else if (parentId == ::OUTBOX) {
        folder = nmo::predefined_phone_msg_folder_outbox::iri();
    } else if (parentId == ::DRAFT) {
        folder = nmo::predefined_phone_msg_folder_draft::iri();
    } else if (parentId == ::SENT) {
        folder = nmo::predefined_phone_msg_folder_sent::iri();
    } else if (parentId == ::MYFOLDER) {
        folder = nmo::predefined_phone_msg_folder_myfolder::iri();
    } else if (parentId > ::MYFOLDER) {
        // TODO: should this be nmo: prefixed?
        folder = QString(LAT("sms-folder-myfolder-0x%1")).arg(parentId, 0, 16);
    }

    if (!folder.isEmpty()) {
        query.deletion(folder, nie::contentLastModified::iri());
        query.insertion(folder, nie::contentLastModified::iri(), LiteralValue(lastModTime));
    }
}

bool TrackerIO::markAsRead(const QList<int> &eventIds)
{
    foreach (int id, eventIds) {
        Live<nmo::Message> msg = d->m_service->liveNode(Event::idToUrl(id));
        if (msg)
            msg->setIsRead(true);
    }
    return true;
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
    int total = -1;
    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::MMSMessage>();
    message.property<nmo::messageId>(LiteralValue(messageToken));
    query.addCountColumn("total", message);
    LiveNodes model = ::tracker()->modelQuery(query);
    if (model->rowCount() > 0) {
        total = model->index(0, 0).data().toInt();
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
