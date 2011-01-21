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

#include <iostream>
#include <QtCore>
#include <QDebug>

#include "../src/groupmodel.h"
#include "../src/conversationmodel.h"
#include "../src/callmodel.h"
#include "../src/draftmodel.h"
#include "../src/event.h"
#include "../src/callevent.h"
#include "../src/group.h"
#include "../src/trackerio.h"

#include <QtTracker/Tracker>
#include <QtTracker/rdfunbound.h>
#include <QtTracker/ontologies/nmo.h>

#include "catcher.h"

using namespace CommHistory;
using namespace SopranoLive;

const char *remoteUids[] = {
    "user@remotehost",
    "user2@remotehost",
    "test.user@gmail.com",
};
const int numRemoteUids = 3;

const char *textContent[] = {
/*00*/ "The quick brown fox jumps over the lazy dog.",
/*01*/ "It was a dark and stormy night.",
/*02*/ "Alussa olivat suo, kuokka ja Jussi.",
/*03*/ "Badger badger mushroom mushroom",
/*04*/ "OMG LOL :D :D :D",
/*05*/ "A $pecial offer for you only today",
/*06*/ "Spam spam lovely spam",
/*07*/ "Hello",
/*08*/ "Lorem ipsum dolor",
/*09*/ "This is a long, really boring message that should span at least a couple of lines and has nothing of value, really nothing, just another typical instant message. :) <- added smiley for FUN!",
/*12*/ "The gnome zaps a wand of death. --More--",
/*11*/ "All work and no play makes Jack a dull boy."
};

const int numTextContents = 12;

const char *mmsSubject[] = {
/*0*/ "Very important message",
/*1*/ "Simple subject",
/*2*/ "READ THIS",
/*3*/ "Funny stuff",
/*4*/ "OMG LOL :D :D :D",
/*5*/ "A $pecial offer for you only today",
/*6*/ "Spam spam lovely spam",
/*7*/ "",
/*8*/ "Lorem ipsum dolor",
/*9*/ "Re: Your previous message",
};

const char*mmsSmil = "<smil><head><layout><root-layout/><region width=\"100%\" height=\"30%\" left=\"0%\" fit=\"scroll\" id=\"Text\" top=\"70%\"/><region width=\"100%\" height=\"70%\" left=\"0%\" fit=\"meet\" id=\"Image\" top=\"0%\"/></layout></head><body><par dur=\"5s\"><text region=\"Text\" src=\"text_slide1\"/><img region=\"Image\" src=\"catphoto\"/></par></body></smil>";

const int numMmsSubjects = 10;

#define TELEPATHY_ACCOUNT_PREFIX       QLatin1String("/org/freedesktop/Telepathy/Account/")
#define TELEPATHY_MMS_ACCOUNT_POSTFIX  QLatin1String("mmscm/mms/mms0")
#define TELEPATHY_RING_ACCOUNT_POSTFIX QLatin1String("ring/tel/ring")

#define MMS_ACCOUNT  TELEPATHY_ACCOUNT_PREFIX + TELEPATHY_MMS_ACCOUNT_POSTFIX
#define RING_ACCOUNT TELEPATHY_ACCOUNT_PREFIX + TELEPATHY_RING_ACCOUNT_POSTFIX

QStringList optionsWithArguments;

QVariantMap parseOptions(QStringList &arguments)
{
    QVariantMap map;
    QMutableListIterator<QString> i(arguments);
    while (i.hasNext()) {
        QString val = i.next();
        if (val.startsWith("-")) {
            i.remove();
            if (optionsWithArguments.contains(val) && i.hasNext()) {
                map.insert(val, i.next());
                i.remove();
            } else {
                map.insert(val, QVariant());
            }
        }
    }

    return map;
}

void printEvent(const Event &event, bool showParts = false)
{
    std::cout << qPrintable(event.toString()) << std::endl;
    if (showParts) {
        foreach (MessagePart part, event.messageParts()) {
            std::cout << "  " << qPrintable(part.toString()) << std::endl;
        }
    }
}

void printUsage()
{
    std::cout << "Usage:"                                                                                                                                  << std::endl;
    std::cout << "commhistory-tool listgroups"                                                                                                             << std::endl;
    std::cout << "                 list [-t] [-p] [-group group-id] [local-uid] [remote-uid]"                                                              << std::endl;
    std::cout << "                 listdrafts"                                                                                                             << std::endl;
    std::cout << "                 listcalls [{bycontact|bytime|bytype|byservice}]"                                                                        << std::endl;
    std::cout << "                 add [-newgroup] [-group group-id] [-date yyyyMMdd:hh:mm] [{-sms|-mms}] [-n number-of-messages] [-async] local-uid remote-uid" << std::endl;
    std::cout << "                 addcall local-uid remote-uid {dialed|missed|received}"                                                                  << std::endl;
    std::cout << "                 addVCard event-id filename label"                                                                                       << std::endl;
    std::cout << "                 addClass0"                                                                                                              << std::endl;
    std::cout << "                 isread event-id {1|0}"                                                                                                  << std::endl;
    std::cout << "                 reportdelivery event-id {1|0}"                                                                                          << std::endl;
    std::cout << "                 setstatus event-id {unknown|sent|sending|delivered|temporarilyfailed|permanentlyfailed}"                                << std::endl;
    std::cout << "                 delete event-id"                                                                                                        << std::endl;
    std::cout << "                 deletegroup group-id"                                                                                                   << std::endl;
    std::cout << "                 deleteall"                                                                                                              << std::endl;
    std::cout << "When adding new events, the default count is 1."                                                                                         << std::endl;
    std::cout << "When adding new events, the given local-ui is ignored, if -sms or -mms specified."                                                       << std::endl;
    std::cout << "New events are of IM type and have random contents."                                                                                     << std::endl;
}

int doAdd(const QStringList &arguments, const QVariantMap &options)
{
    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    QString localUid = arguments.at(2);
    QString remoteUid = arguments.at(3);
    int groupId = -1;

    int count = 1;
    if (options.contains("-n")) {
        count = options.value("-n").toInt();
        if (count <= 0) {
            qCritical() << "Invalid number of messages";
            return -1;
        }
    }

    if (!localUid.startsWith(TELEPATHY_ACCOUNT_PREFIX)) {
        localUid.prepend(TELEPATHY_ACCOUNT_PREFIX);
    }

    if (options.contains("-newgroup")) {
        Group group;
        group.setLocalUid(localUid);
        QStringList remoteUids;
        remoteUids << remoteUid;
        group.setRemoteUids(remoteUids);
        if (!groupModel.addGroup(group)) {
            qCritical() << "Error adding group:" << groupModel.lastError();
            return -1;
        }
        groupId = group.id();
        std::cout << "Added group " << groupId << std::endl;
    }

    if (options.contains("-group")) {
        bool ok = false;
        groupId = options.value("-group").toInt(&ok);
        if (!ok) {
            qCritical() << "Invalid group id";
            return -1;
        }
    }

    bool isSms = options.contains("-sms");
    bool isMms = options.contains("-mms");

    QDateTime date = QDateTime::currentDateTime();
    if (options.contains("-date")) {
        date = QDateTime::fromString(options.value("-date").toString(), "yyyyMMdd:hh:mm");
        if (!date.isValid()) {
            qCritical() << "Invalid date";
            return -1;
        }
    }

    qsrand(QDateTime::currentDateTime().toTime_t());
    bool randomRemote = true;
    if (arguments.count() > 3) {
        remoteUid = arguments.at(3);
        randomRemote = false;
    }

    EventModel model;
    QList<Event> events;
    for (int i = 0; i < count; i++) {
        Event e;

        e.setDirection((Event::EventDirection)((qrand() & 1) + 1));

        if (isMms)
        {
            e.setType(Event::MMSEvent);
            e.setLocalUid(MMS_ACCOUNT);
            e.setSubject(mmsSubject[qrand() % numMmsSubjects]);

            if(e.direction() == Event::Outbound || qrand() % 2 == 0)
            {
                //create full MMS
                e.setContentLocation(QString());

                if(qrand() % 2 == 0)
                    e.setCcList(QStringList() << "111111" << "222222" << "iam@cc.list.com");
                if(qrand() % 2 == 0)
                    e.setBccList(QStringList() << "33333" << "44444" << "iam@bcc.list.com");

                QList<MessagePart> parts;

                bool smilAdded = false;
                if (qrand() % 2 == 0)
                {
                    MessagePart part1;
                    part1.setContentType("application/smil");
                    part1.setPlainTextContent(mmsSmil);
                    parts << part1;
                    smilAdded = true;
                }
                MessagePart part2;
                part2.setContentId("text_slide1");
                part2.setContentType("text/plain");
                part2.setPlainTextContent(textContent[qrand() % numTextContents]);
                parts << part2;
                if (smilAdded || qrand() % 3 == 0)
                {
                    MessagePart part3;
                    part3.setContentId("catphoto");
                    part3.setContentType("image/jpeg");
                    part3.setContentSize(101000);
                    part3.setContentLocation("/home/user/.mms/msgid001/catphoto.jpg");
                    parts << part3;
                }
                e.setMessageParts(parts);
            }
            else
            {
                //create MMS notification
                e.setContentLocation("http://dummy.mmsc.com/mms");
            }
        }
        else if (isSms)
        {
            e.setType(Event::SMSEvent);
            e.setLocalUid(RING_ACCOUNT);
        }
        else
        {
            e.setType(Event::IMEvent);
            e.setLocalUid(localUid);
        }

        e.setStartTime(date);
        e.setEndTime(date);
        if (e.direction() == Event::Outbound) {
            e.setIsRead(true);
        } else {
            e.setIsRead(false);
        }
        e.setBytesSent(qrand() % 1024);
        e.setBytesReceived(qrand() % 1024);
        e.setGroupId(groupId);
        if (randomRemote) {
            e.setRemoteUid(remoteUids[qrand() % numRemoteUids]);
        } else {
            e.setRemoteUid(remoteUid);
        }
        e.setFreeText(textContent[qrand() % numTextContents]);

        events.append(e);
    }

    Catcher c(&model);

    if (!model.addEvents(events, false)) {
        qCritical() << "Error adding events:" << model.lastError();
        return -1;
    }

    c.waitCommit(events.count());

    return 0;
}

int doAddCall( const QStringList &arguments, const QVariantMap &options )
{
    Q_UNUSED( options )

    qsrand( QDateTime::currentDateTime().toTime_t() );

    int count = 1;
    if (options.contains("-n")) {
        count = options.value("-n").toInt();
        if (count <= 0) {
            qCritical() << "Invalid number of calls";
            return -1;
        }
    }

    QString localUid = arguments.at(2);
    if (!localUid.startsWith(TELEPATHY_ACCOUNT_PREFIX)) {
        localUid.prepend(TELEPATHY_ACCOUNT_PREFIX);
    }

    QList<Event> events;
    for (int i = 0; i < count; i++) {
        Event e;
        e.setType( Event::CallEvent );
        e.setStartTime( QDateTime::currentDateTime() );
        e.setEndTime( QDateTime::currentDateTime() );
        e.setLocalUid( localUid );
        e.setGroupId( -1 );

        int callType = -1;
        if (arguments.last() == "dialed") {
            callType = 0;
        } else if (arguments.last() == "missed") {
            callType = 1;
        } else if (arguments.last() == "received") {
            callType = 2;
        }

        if (callType == -1)
            callType = qrand() % 3;

        Event::EventDirection direction = Event::Inbound;
        bool isMissed = false;
        if (callType == 0) {
            direction = Event::Outbound;
        } else if (callType == 1) {
            direction = Event::Inbound;
            isMissed = true;
        }

        if (arguments.count() > 4) {
            e.setRemoteUid(arguments.at(3));
        } else {
            e.setRemoteUid(remoteUids[0]);
        }
        e.setDirection( direction );
        e.setIsMissedCall( isMissed );

        events.append(e);
    }
    EventModel model;
    Catcher c(&model);

    if (!model.addEvents(events, false)) {
        qCritical() << "Error adding event:" << model.lastError();
        return -1;
    }

    c.waitCommit(events.count());

    return 0;
}

int doAddVCard(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(options)

    bool conversionSuccess = false;
    int id = arguments.at(2).toInt(&conversionSuccess);
    if(!conversionSuccess) {
        qCritical() << "Invalid event id";
        return -1;
    }
    EventModel model;
    Event e;
    if(!model.trackerIO().getEvent(id, e)) {
        qCritical() << "Error getting event" << id << ":" << model.trackerIO().lastError();
        return -1;
    }
    e.setFromVCard(arguments.at(3), arguments.at(4));

    Catcher c(&model);

    if (!model.modifyEvent(e)) {
        qCritical() << "Error modifying event" << id << ":"
                    << model.lastError();
        return -1;
    }

    c.waitCommit();

    return 0;
}

int doAddClass0(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(arguments)
    Q_UNUSED(options)

    QStringList sosRemoteUids;
    sosRemoteUids << "112"
                  << "911"
                  << "POLIZEI";

    QStringList sosMessages;
    sosMessages << "Don't fall asleep, Freddy is around."
                << "Stay inside, storm is coming."
                << "Class Zero SMS"
                << "The Steelers has won the Superbowl. Party-time!";

    // prepare class zero sms
    Event e;
    e.setRemoteUid(sosRemoteUids.at(qrand() % sosRemoteUids.count()));
    e.setLocalUid(RING_ACCOUNT);
    e.setDirection(Event::Inbound);
    e.setType(Event::ClassZeroSMSEvent);
    e.setFreeText(sosMessages.at(qrand() % sosMessages.count()));
    QDateTime now = QDateTime::currentDateTime();
    e.setStartTime(now);
    e.setEndTime(now);

    EventModel model;
    if (!model.addEvent(e, true)) {

        qCritical() << "Error adding events:" << model.lastError();
        return -1;
    }
    qDebug() << "Added Class Zero SMS.";

    return 0;
}

int doList(const QStringList &arguments, const QVariantMap &options)
{
    QString localUid;
    QString remoteUid;
    int groupId = -1;

    if (options.contains("-group")) {
        bool ok = false;
        groupId = options.value("-group").toInt(&ok);
        if (!ok) {
            qCritical() << "Invalid group id";
            return -1;
        }
    }

    if (groupId == -1) {
        qCritical() << "Not implemented, use list -group <id>";
        return -1;
    }

    bool tree = options.contains("-t");
    bool showParts = options.contains("-p");

    if (arguments.count() > 2) {
        localUid = arguments.at(2);
    }
    if (arguments.count() > 3) {
        remoteUid = arguments.at(3);
    }

    Group::ChatType chatType = Group::ChatTypeP2P;
    Live<nmo::CommunicationChannel> conversation = ::tracker()->strictLiveNode(Group::idToUrl(groupId));
    if (conversation)
        chatType = (Group::ChatType)conversation->getIdentifier().toUInt();

    ConversationModel model;
    model.enableContactChanges(false);
    model.setQueryMode(EventModel::SyncQuery);
    model.setTreeMode(tree);
    if (!model.getEventsWithType(groupId, chatType)) {
        qCritical() << "Error fetching events:" << model.lastError();
        return -1;
    }

    if (!tree) {
        for (int i = 0; i < model.rowCount(); i++) {
            Event e = model.event(model.index(i, 0));
            printEvent(e, showParts);
        }
    } else {
        for (int i = 0; i < model.rowCount(); i++) {
            QModelIndex parent = model.index(i, 0);
            if (model.hasChildren(parent)) {
                QString header = "*** " + model.event(parent).freeText() + " ***";
                std::cout << qPrintable(header) << std::endl;
                for (int row = 0; row < model.rowCount(parent); row++) {
                    Event e = model.event(model.index(row, 0, parent));
                    printEvent(e, showParts);
                }
            }
        }
    }

    return 0;
}

int doListGroups(const QStringList &arguments, const QVariantMap &options)
{
    QString localUid;
    QString remoteUid;

    if (arguments.count() > 2) {
        localUid = arguments.at(2);
        if (!localUid.isEmpty() && !localUid.startsWith(TELEPATHY_ACCOUNT_PREFIX)) {
            localUid.prepend(TELEPATHY_ACCOUNT_PREFIX);
        }
    }

    if (arguments.count() > 3) {
        remoteUid = arguments.at(3);
    }

    bool showParts = options.contains("-p");

    GroupModel model;
    model.enableContactChanges(false);
    model.setQueryMode(EventModel::SyncQuery);
    if (!model.getGroups(localUid, remoteUid)) {
        qCritical() << "Error fetching groups:" << model.lastError();
        return -1;
    }
    EventModel eventModel;
    for (int i = 0; i < model.rowCount(); i++) {
        Group g = model.group(model.index(i, 0));
        QString line =
            QString("Group %1 (%2 messages, %3 unread, %4 sent) %5 %6 %7 %8")
                .arg(g.id())
                .arg(g.totalMessages())
                .arg(g.unreadMessages())
                .arg(g.sentMessages())
                .arg(g.chatName())
                .arg(g.remoteUids().join("|"))
                .arg(g.contactId())
                .arg(g.contactName());
        std::cout << qPrintable(line) << std::endl;
        Event e;
        if (eventModel.trackerIO().getEvent(g.lastEventId(), e)) {
            printEvent(e, showParts);
        } else {
            std::cout << "getEvent error " << qPrintable(eventModel.trackerIO().lastError().text()) << std::endl;
        }

        std::cout << std::endl;
    }

    return 0;
}

int doListCalls( const QStringList &arguments, const QVariantMap &options )
{
    Q_UNUSED( options );

    CallModel model;
    model.enableContactChanges(false);
    model.setQueryMode(EventModel::SyncQuery);
    CallModel::Sorting sorting = CallModel::SortByContact;

    if ( arguments.count() == 3 )
    {
        if ( arguments.at( 2 ) == "bytime" )
        {
            sorting = CallModel::SortByTime;
        }
        else if ( arguments.at( 2 ) ==  "bytype" )
        {
            sorting = CallModel::SortByType;
        }
        else if ( arguments.at( 2 ) ==  "byservice" )
        {
            sorting = CallModel::SortByService;
        }
    }

    model.setFilter(sorting);
    if ( !model.getEvents() )
    {
        qCritical() << "Error fetching events:" << model.lastError();
        return -1;
    }

    for ( int i = 0; i < model.rowCount(); i++ )
    {
        Event e = model.event(model.index(i, 0));
        printEvent(e);
    }

    return 0;
}

int doIsRead(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(options);

    bool ok = false;
    int id = arguments.at(2).toInt(&ok);
    if (!ok) {
        qCritical() << "Invalid event id";
        return -1;
    }
    bool isRead = true;
    if (arguments.count() > 3) {
        if (arguments.at(3).toUInt() != 0 ||
            arguments.at(3).startsWith("t")) {
            isRead = true;
        } else {
            isRead = false;
        }
    }

    EventModel model;
    Event event;
    if (!model.trackerIO().getEvent(id, event)) {
        qCritical() << "Error getting event" << id << ":" << model.trackerIO().lastError();
        return -1;
    }


    event.setIsRead(isRead);
    if (!model.modifyEvent(event)) {
        qCritical() << "Error updating event" << event.id() << ":"
                    << model.lastError();
        return -1;
    }

    return 0;
}

int doReportDelivery(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(options);

    bool ok = false;
    int id = arguments.at(2).toInt(&ok);
    if (!ok) {
        qCritical() << "Invalid event id";
        return -1;
    }

    bool reportDelivery = false;
    if (arguments.count() > 3) {
        reportDelivery = (arguments.at(3).toUInt() != 0);
    }

    EventModel model;
    Event event;
    if (!model.trackerIO().getEvent(id, event)) {
        qCritical() << "Error getting event" << id << ":" << model.trackerIO().lastError();
        return -1;
    }

    event.setReportDelivery(reportDelivery);

    Catcher c(&model);

    if (!model.modifyEvents(QList<Event>() << event)) {
        qCritical() << "Error modifying event" << id << ":"
                    << model.lastError();
        return -1;
    }

    c.waitCommit();

    return 0;
}

int doSetStatus(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(options);

    bool ok = false;
    int id = arguments.at(2).toInt(&ok);
    if (!ok) {
        qCritical() << "Invalid event id";
        return -1;
    }

    EventModel model;
    Event event;
    if (!model.trackerIO().getEvent(id, event)) {
        qCritical() << "Error getting event" << id << ":" << model.trackerIO().lastError();
        return -1;
    }

    Event::EventStatus status;
    if ( arguments.count() == 4 && arguments.at( 3 ) == "sending" )
    {
        status = Event::SendingStatus;
    }
    else if ( arguments.count() == 4 && arguments.at( 3 ) == "sent" )
    {
        status = Event::SentStatus;
    }
    else if ( arguments.count() == 4 && arguments.at( 3 ) == "delivered" )
    {
        status = Event::DeliveredStatus;
    }
    else if ( arguments.count() == 4 && arguments.at( 3 ) == "temporarilyfailed" )
    {
        status = Event::TemporarilyFailedStatus;
    }
    else if ( arguments.count() == 4 && arguments.at( 3 ) == "permanentlyfailed" )
    {
        status = Event::PermanentlyFailedStatus;
    }
    else
    {
        status = Event::UnknownStatus;
    }

    qDebug() << "Old status: " << event.status();
    event.setStatus( status );
    qDebug() << "New status: " << event.status();

    Catcher c(&model);

    if (!model.modifyEvents(QList<Event>() << event)) {
        qCritical() << "Error modifying event" << id << ":" << model.lastError();
        return -1;
    }
    c.waitCommit();

    return 0;
}

int doDelete(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(options);

    bool ok = false;
    int id = arguments.at(2).toInt(&ok);
    if (!ok) {
        qCritical() << "Invalid event id";
        return -1;
    }

    EventModel model;
    if (!model.deleteEvent(id)) {
        qCritical() << "Error deleting event" << id << ":" << model.lastError();
        return -1;
    }
    return 0;
}

int doDeleteGroup(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(options);

    bool ok = false;
    int id = arguments.at(2).toInt(&ok);
    if (!ok) {
        qCritical() << "Invalid group id";
        return -1;
    }

    GroupModel model;
    if (!model.deleteGroups(QList<int>() << id)) {
        qCritical() << "Error deleting group" << id << ":" << model.lastError();
        return -1;
    }

    return 0;
}

int doDeleteAll(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(arguments);
    Q_UNUSED(options);

    RDFUpdate deleter;
    deleter.addDeletion(RDFVariable::fromType<nmo::Message>(),
                        rdf::type::iri(), rdfs::Resource::iri());
    deleter.addDeletion(RDFVariable::fromType<nmo::CommunicationChannel>(),
                        rdf::type::iri(), rdfs::Resource::iri());
    ::tracker()->executeQuery(deleter);

    return 0;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    optionsWithArguments << "-group" << "-date" << "-n";

    QStringList args = app.arguments();
    QVariantMap options = parseOptions(args);

    if (args.count() < 2) {
        printUsage();
        exit(0);
    }

    if (args.at(1) == "add" && args.count() > 3) {
        return doAdd(args, options);
    } else if (args.at(1) == "addcall" && args.count() >= 3) {
        return doAddCall( args, options );
    } else if (args.at(1) == "addVCard") {
        return doAddVCard(args, options);
    } else if (args.at(1) == "addClass0") {
        return doAddClass0(args, options);
    } else if (args.at(1) == "listcalls" &&
               (args.count() == 2 ||
                (args.count() == 3 && (args.at(2) == "bycontact" ||
                                       args.at(2) == "bytime"    ||
                                       args.at(2) == "bytype"    ||
                                       args.at(2) == "byservice")))) {
        return doListCalls(args, options);
    } else if (args.at(1) == "list") {
        return doList(args, options);
    } else if (args.at(1) == "listgroups") {
        return doListGroups(args, options);
    } else if (args.at(1) == "isread" && args.count() > 2) {
        return doIsRead(args, options);
    } else if (args.at(1) == "reportdelivery" && args.count() > 2) {
        return doReportDelivery(args, options);
    } else if (args.at(1) == "setstatus" &&
               (args.count() == 4 && (args.at(3) == "unknown"   ||
                                      args.at(3) == "sending"   ||
                                      args.at(3) == "sent"      ||
                                      args.at(3) == "delivered" ||
                                      args.at(3) == "temporarilyfailed" ||
                                      args.at(3) == "permanentlyfailed"))) {
        return doSetStatus( args, options );
    } else if (args.at(1) == "delete" && args.count() > 2) {
        return doDelete(args, options);
    } else if (args.at(1) == "deletegroup" && args.count() > 2) {
        return doDeleteGroup(args, options);
    } else if (args.at(1) == "deleteall") {
        return doDeleteAll(args, options);
    } else {
        printUsage();
    }

    return 0;
}
