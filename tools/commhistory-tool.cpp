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

#include <iostream>
#include <QtCore>
#include <QDebug>
#include <QUuid>

#include "../src/groupmodel.h"
#include "../src/conversationmodel.h"
#include "../src/callmodel.h"
#include "../src/event.h"
#include "../src/callevent.h"
#include "../src/group.h"
#include "../src/databaseio.h"

#include "catcher.h"

#include <QJsonDocument>

using namespace CommHistory;

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
#define TELEPATHY_RING_ACCOUNT_POSTFIX QLatin1String("ring/tel/account0")

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
            std::cout << "  " << qPrintable(part.debugString()) << std::endl;
        }
    }
}

void printUsage()
{
    std::cout << "Usage:"                                                                                                                                  << std::endl;
    std::cout << "commhistory-tool listgroups"                                                                                                             << std::endl;
    std::cout << "                 list [-t] [-p] [-group group-id] [local-uid] [remote-uid]"                                                              << std::endl;
    std::cout << "                 listcalls [{bycontact|bytime|bytype|byservice}]"                                                                        << std::endl;
    std::cout << "                 add [-newgroup] [-group group-id] [-startTime yyyyMMdd:hh:mm] [-endTime yyyyMMdd:hh:mm] [{-sms|-mms}] [{-in|-out}] [-n number-of-messages] [-async] [-text message-text] local-uid remote-uid" << std::endl;
    std::cout << "                 addcall local-uid remote-uid {dialed|missed|received}"                                                                  << std::endl;
    std::cout << "                 addVCard event-id filename label"                                                                                       << std::endl;
    std::cout << "                 addClass0"                                                                                                              << std::endl;
    std::cout << "                 isread event-id {1|0}"                                                                                                  << std::endl;
    std::cout << "                 isvideo event-id {1|0}"                                                                                                 << std::endl;
    std::cout << "                 reportdelivery event-id {1|0}"                                                                                          << std::endl;
    std::cout << "                 setstatus event-id {unknown|sent|sending|delivered|temporarilyfailed|permanentlyfailed}"                                << std::endl;
    std::cout << "                 delete event-id"                                                                                                        << std::endl;
    std::cout << "                 deletegroup group-id"                                                                                                   << std::endl;
    std::cout << "                 deleteall [-groups] [-calls] [-reset]"                                                                                  << std::endl;
    std::cout << "                 markallcallsread"                                                                                                       << std::endl;
    std::cout << "                 export [-group group-id] [-calls] [-groups] filename"
                        << std::endl;
    std::cout << "                 import filename"
                        << std::endl;
    std::cout << "                 import-json [-relativeDate yyMMdd] filename"
                        << std::endl;
    std::cout << "When adding new events, the default count is 1."                                                                                         << std::endl;
    std::cout << "When adding new events, the given local-ui is ignored, if -sms or -mms specified."                                                       << std::endl;
    std::cout << "New events are of IM type and have random contents."                                                                                     << std::endl;
}

int doAdd(const QStringList &arguments, const QVariantMap &options)
{
    GroupModel groupModel;
    groupModel.enableContactChanges(false);

    bool isSms = options.contains("-sms");
    bool isMms = options.contains("-mms");

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

    // use predefined accounts for sms/mms
    if (isSms) {
        localUid = RING_ACCOUNT;
    }
    else if (isMms) {
        localUid = MMS_ACCOUNT;
    }

    if (!localUid.startsWith(TELEPATHY_ACCOUNT_PREFIX)) {
        localUid.prepend(TELEPATHY_ACCOUNT_PREFIX);
    }


    QDateTime startTime = QDateTime::currentDateTime();
    if (options.contains("-startTime")) {
        startTime = QDateTime::fromString(options.value("-startTime").toString(), "yyyyMMdd:hh:mm");
        if (!startTime.isValid()) {
            qCritical() << "Invalid start time";
            return -1;
        }
    }

    QDateTime endTime = startTime;
    if (options.contains("-endTime")) {
        endTime = QDateTime::fromString(options.value("-endTime").toString(), "yyyyMMdd:hh:mm");
        if (!endTime.isValid()) {
            qCritical() << "Invalid end time";
            return -1;
        }
        if (!options.contains("-startTime"))
            startTime = endTime;
    }

    if (options.contains("-newgroup")) {
        Group group;
        group.setLocalUid(localUid);
        group.setRecipients(RecipientList::fromUids(localUid, QStringList() << remoteUid));
        if (!groupModel.addGroup(group)) {
            qCritical() << "Error adding group";
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

    QString messageText;
    if (options.contains("-text")) {
        messageText = options.value("-text").toString();
    }

    Event::EventDirection direction = Event::UnknownDirection;
    if (options.contains("-in"))
        direction = Event::Inbound;
    if (options.contains("-out"))
        direction = Event::Outbound;

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

        if (direction == Event::UnknownDirection)
            e.setDirection((Event::EventDirection)((qrand() & 1) + 1));
        else
            e.setDirection(direction);

        if (isMms)
        {
            e.setType(Event::MMSEvent);
            e.setLocalUid(localUid);
            e.setSubject(mmsSubject[qrand() % numMmsSubjects]);
            e.setMessageToken(QUuid::createUuid().toString());

            if(e.direction() == Event::Outbound || qrand() % 2 == 0)
            {
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
                    parts << part1;
                    smilAdded = true;
                }
                MessagePart part2;
                part2.setContentId("text_slide1");
                part2.setContentType("text/plain");
                parts << part2;
                if (smilAdded || qrand() % 3 == 0)
                {
                    MessagePart part3;
                    part3.setContentId("catphoto");
                    part3.setContentType("image/jpeg");
                    part3.setPath("/home/user/.mms/msgid001/catphoto.jpg");
                    parts << part3;
                }
                e.setMessageParts(parts);
            }
        }
        else if (isSms)
        {
            e.setType(Event::SMSEvent);
            e.setLocalUid(localUid);
            e.setMessageToken(QUuid::createUuid().toString());
        }
        else
        {
            e.setType(Event::IMEvent);
            e.setLocalUid(localUid);
        }

        e.setStartTime(startTime);
        e.setEndTime(endTime);
        if (e.direction() == Event::Outbound) {
            e.setIsRead(true);
        } else {
            e.setIsRead(false);
        }
        e.setBytesReceived(qrand() % 1024);
        e.setGroupId(groupId);
        if (randomRemote) {
            e.setRemoteUid(remoteUids[qrand() % numRemoteUids]);
        } else {
            e.setRemoteUid(remoteUid);
        }

        e.setFreeText(messageText.isNull() ? textContent[qrand() % numTextContents] : messageText);

        events.append(e);
    }

    Catcher c(&model);

    if (!model.addEvents(events, false)) {
        qCritical() << "Error adding events";
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
        qCritical() << "Error adding event";
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
    if(!model.databaseIO().getEvent(id, e)) {
        qCritical() << "Error getting event" << id;
        return -1;
    }
    e.setFromVCard(arguments.at(3), arguments.at(4));

    Catcher c(&model);

    if (!model.modifyEvent(e)) {
        qCritical() << "Error modifying event" << id;
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

        qCritical() << "Error adding events";
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

    ConversationModel model;
    model.setQueryMode(EventModel::SyncQuery);
    model.setTreeMode(tree);
    if (!model.getEvents(groupId)) {
        qCritical() << "Error fetching events";
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
        qCritical() << "Error fetching groups";
        return -1;
    }

    Q_UNUSED(showParts);
    EventModel eventModel;
    for (int i = 0; i < model.rowCount(); i++) {
        Group g = model.group(model.index(i, 0));
        std::cout << qPrintable(g.toString()) << std::endl;

        Event e;
        if (eventModel.databaseIO().getEvent(g.lastEventId(), e)) {
            printEvent(e, showParts);
        } else {
            qCritical() << "getEvent error ";
        }

        std::cout << std::endl;
    }

    return 0;
}

int doListCalls( const QStringList &arguments, const QVariantMap &options )
{
    Q_UNUSED( options );

    CallModel model;
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
        qCritical() << "Error fetching events";
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
    if (!model.databaseIO().getEvent(id, event)) {
        qCritical() << "Error getting event" << id;
        return -1;
    }


    event.setIsRead(isRead);

    Catcher c(&model);
    if (!model.modifyEvent(event)) {
        qCritical() << "Error updating event" << event.id();
        return -1;
    }
    c.waitCommit();

    return 0;
}

int doIsVideo(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(options);

    bool ok = false;
    int id = arguments.at(2).toInt(&ok);
    if (!ok) {
        qCritical() << "Invalid event id";
        return -1;
    }
    bool isVideo = true;
    if (arguments.count() > 3) {
        if (arguments.at(3).toUInt() != 0 ||
            arguments.at(3).startsWith("t")) {
            isVideo = true;
        } else {
            isVideo = false;
        }
    }

    CallModel model;
    Event event;
    if (!model.databaseIO().getEvent(id, event)) {
        qCritical() << "Error getting event" << id;
        return -1;
    }


    event.setIsVideoCall(isVideo);

    Catcher c(&model);
    if (!model.modifyEvent(event)) {
        qCritical() << "Error updating event" << event.id();
        return -1;
    }
    c.waitCommit();

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
    if (!model.databaseIO().getEvent(id, event)) {
        qCritical() << "Error getting event" << id;
        return -1;
    }

    event.setReportDelivery(reportDelivery);

    Catcher c(&model);

    if (!model.modifyEvents(QList<Event>() << event)) {
        qCritical() << "Error modifying event" << id;
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
    if (!model.databaseIO().getEvent(id, event)) {
        qCritical() << "Error getting event" << id;
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
        qCritical() << "Error modifying event" << id;
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
    Catcher c(&model);

    if (!model.deleteEvent(id)) {
        qCritical() << "Error deleting event" << id;
        return -1;
    }
    c.waitCommit();

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
    Catcher c(&model);

    if (!model.deleteGroups(QList<int>() << id)) {
        qCritical() << "Error deleting group" << id;
        return -1;
    }
    c.waitCommit(0);

    return 0;
}

int doDeleteAll(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(arguments);
    Q_UNUSED(options);

    bool hasAnyOption = options.size() > 0;

    if (!hasAnyOption || options.contains("-groups")) {
        GroupModel model;
        model.enableContactChanges(false);
        model.setQueryMode(EventModel::SyncQuery);
        if (!model.getGroups()) {
            qCritical() << "Error fetching groups";
            return -1;
        }

        if (model.rowCount()) {
            Catcher c(&model);
            model.deleteAll();
            c.waitCommit(0);
        }
    }

    if (!hasAnyOption || options.contains("-calls")) {
        CallModel callModel;
        callModel.setTreeMode(false);
        callModel.setFilter(CallModel::SortByTime);
        callModel.setQueryMode(EventModel::SyncQuery);
        if (!callModel.getEvents()) {
            qCritical() << "Error fetching calls";
            return -1;
        }

        if (callModel.rowCount()) {
            Catcher c(&callModel);
            callModel.deleteAll();
            c.waitCommit(0);
        }
    }

    if (!hasAnyOption || options.contains("-reset")) {
        DatabaseIO::instance()->deleteAllEvents(Event::UnknownType);
        qWarning() << "Other clients must be restarted to refresh their view of the events database";
    }

    return 0;
}

int doMarkAllCallsRead(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(arguments);
    Q_UNUSED(options);

    CallModel model;
    Catcher c(&model);

    if (!model.markAllRead()) {
        qCritical() << "Error marking all calls as read.";
        return -1;
    }
    c.waitCommit(0);

    return 0;
}

bool exportGroup(QDataStream &out, const Group &group)
{
    ConversationModel model;
    model.setQueryMode(EventModel::SyncQuery);
    if (!model.getEvents(group.id())) {
        qWarning() << "Error reading events from group" << group.id();
        return false;
    }

    out << group << model.rowCount();
    for (int i = 0; i < model.rowCount(); i++) {
        Event event = model.event(model.index(i, 0));
        out << event;
    }

    return true;
}

int doExport(const QStringList &arguments, const QVariantMap &options)
{
    QString fileName = arguments.at(2);
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << "Unable to open file" << fileName << " for writing:" << file.errorString();
        return -1;
    }

    // extremely sophisticated stream format:
    // numberOfGroups
    //   group 1
    //   numberOfEvents in group 1
    //   events
    //   group 2
    //   numberOfEvents in group 2
    //   events of group 2
    //   ...
    // numberOfCalls
    //   calls
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_4_7);

    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    groupModel.setQueryMode(EventModel::SyncQuery);

    if (options.contains("-group")) {
        bool ok = false;
        int id = options.value("-group").toInt(&ok);
        if (!ok) {
            qCritical() << "Invalid group id";
            return -1;
        }

        Group group;
        if (!groupModel.databaseIO().getGroup(id, group)) {
            qCritical() << "Error reading group" << id;
            return -1;
        }
        out << 1;
        exportGroup(out, group);
    } else if (options.contains("-groups")) {
        if (!groupModel.getGroups()) {
            qCritical() << "Error reading groups";
            return -1;
        }

        out << groupModel.rowCount();
        for (int i = 0; i < groupModel.rowCount(); i++)
            exportGroup(out, groupModel.group(groupModel.index(i, 0)));
    } else {
        out << 0;
    }

    if (options.contains("-calls")) {
        CallModel callModel;
        callModel.setTreeMode(false);
        callModel.setFilter(CallModel::SortByTime);
        callModel.setQueryMode(EventModel::SyncQuery);
        if (!callModel.getEvents()) {
            qCritical() << "Error reading calls";
            return -1;
        }

        out << callModel.rowCount();
        for (int i = 0; i < callModel.rowCount(); i++)
            out << callModel.event(callModel.index(i, 0));
    } else {
        out << 0;
    }

    return 0;
}

int doImport(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(options);

    QString fileName = arguments.at(2);
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Unable to open file" << fileName << " for reading:" << file.errorString();
        return -1;
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_4_7);

    int totalEvents = 0;
    int numGroups;
    in >> numGroups;
    if (numGroups) {
        GroupModel groupModel;
        groupModel.enableContactChanges(false);
        groupModel.setQueryMode(EventModel::SyncQuery);
        Catcher groupCatcher(&groupModel);
        EventModel model;
        Catcher eventCatcher(&model);

        for (int i = 0; i < numGroups; i++) {
            Group group;
            in >> group;
            int numEvents;
            in >> numEvents;
            bool ok = true;
            groupCatcher.reset();
            if (!groupModel.addGroup(group)) {
                qWarning() << "Error adding group ( local"
                           << group.localUid() << ", remote" << group.recipients().debugString() << ")";
                ok = false;
            }
            groupCatcher.waitCommit(0);

            QList<Event> events;
            for (int j = 0; j < numEvents; j++) {
                Event event;
                in >> event;
                event.setGroupId(group.id());
                events << event;
            }

            if (ok) {
                eventCatcher.reset();
                if (!model.addEvents(events)) {
                    qWarning() << "Error adding events for group" << group.id();
                    continue;
                }
                eventCatcher.waitCommit(numEvents);
                totalEvents += numEvents;
            }
        }
    }

    int numCalls;
    in >> numCalls;
    if (numCalls) {
        EventModel model;
        model.setQueryMode(EventModel::SyncQuery);
        Catcher catcher(&model);

        QList<Event> events;
        for (int i = 0; i < numCalls; i++) {
            Event event;
            in >> event;
            events << event;
        }
        if (!model.addEvents(events)) {
            qWarning() << "Error adding calls";
        } else {
            catcher.waitCommit(numCalls);
        }
    }

    std::cout << "Imported " << numGroups << " conversations, "
              << totalEvents << " messages, "
              << numCalls << " calls" << std::endl;

    return 0;
}

int doJsonImport(const QStringList &arguments, const QVariantMap &options)
{
    Q_UNUSED(options);

    QString fileName = arguments.at(2);
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Unable to open file" << fileName << " for reading:" << file.errorString();
        return -1;
    }

    int dateOffset = 0;
    if (options.contains("-relativeDate")) {
        QDateTime endTime = QDateTime::fromString(options.value("-relativeDate").toString(), "yyyyMMdd");
        if (!endTime.isValid()) {
            qCritical() << "Invalid end time";
            return -1;
        }

        dateOffset = endTime.secsTo(QDateTime(QDate::currentDate()));
    }

    bool ok = true;

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument json = QJsonDocument::fromJson(data, &error);
    if (json.isNull()) {
        qCritical() << "Unable to import file" << fileName << ":" << error.errorString() << "at" << error.offset;
        return -1;
    }
    QVariantList result = json.array().toVariantList();

    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    groupModel.setQueryMode(EventModel::SyncQuery);
    Catcher groupCatcher(&groupModel);
    EventModel model;
    Catcher eventCatcher(&model);
    int groupCount = 0;

    foreach (const QVariant &c, result) {
        QVariantMap conversation = c.toMap();
        Group group;
        Event::EventType type;

        groupCount++;

        if (conversation["type"] == QLatin1String("sms")) {
            type = Event::SMSEvent;
            group.setLocalUid(RING_ACCOUNT);
        } else if (conversation["type"] == QLatin1String("im")) {
            type = Event::IMEvent;
            QString from = conversation["from"].toString();
            if (from.isEmpty()) {
                qWarning() << "No 'from' field in IM conversation" << groupCount;
                ok = false;
                continue;
            }

            group.setLocalUid(TELEPATHY_ACCOUNT_PREFIX + from);
        } else if (conversation["type"] == QLatin1String("call")) {
            type = Event::CallEvent;
            group.setLocalUid(RING_ACCOUNT);
        } else {
            qWarning() << "No valid type for conversation" << groupCount;
            ok = false;
            continue;
        }

        QString to = conversation.value("to").toString();
        if (to.isEmpty()) {
            qWarning() << "No 'to' field in conversation" << groupCount;
            ok = false;
            continue;
        }

        group.setRecipients(RecipientList::fromUids(group.localUid(), QStringList() << to));
        group.setChatType(Group::ChatTypeP2P);

        // Calls don't actually belong to groups
        if (type != Event::CallEvent) {
            groupCatcher.reset();
            if (!groupModel.addGroup(group)) {
                qWarning() << "Error adding conversation" << groupCount << "( local" << group.localUid() << ", remote" << group.recipients().debugString() << ")";
                ok = false;
                continue;
            }
            groupCatcher.waitCommit(0);
        }

        // events (and "messages" for compatibility)
        QVariantList eventData;
        eventData << conversation.value("events").toList() << conversation.value("messages").toList();
        QList<Event> events;
        events.reserve(eventData.size());

        int eventCount = 0;
        foreach (const QVariant &e, eventData) {
            QVariantMap data = e.toMap();
            Event event;
            event.setType(type);
            if (group.id() >= 0)
                event.setGroupId(group.id());
            event.setLocalUid(group.localUid());
            event.setRemoteUid(group.recipients().remoteUids().first());

            eventCount++;

            if (data["direction"] == "in") {
                event.setDirection(Event::Inbound);
            } else if (data["direction"] == "out") {
                event.setDirection(Event::Outbound);
                event.setStatus(Event::DeliveredStatus);
            } else {
                qWarning() << "No valid direction for event" << eventCount << "in conversation" << groupCount;
                ok = false;
                continue;
            }

            QDateTime date = QDateTime::fromString(data.value("date").toString(), Qt::ISODate);
            if (!date.isValid()) {
                qWarning() << "No valid date for event" << eventCount << "in conversation" << groupCount;
                ok = false;
                continue;
            }

            QDateTime endDate = QDateTime::fromString(data.value("endDate").toString(), Qt::ISODate);
            if (!endDate.isValid())
                endDate = date;

            date = date.addSecs(dateOffset);
            endDate = endDate.addSecs(dateOffset);
            event.setStartTime(date);
            event.setEndTime(endDate);

            if (type == Event::CallEvent) {
                if (data.value("missed").toBool())
                    event.setIsMissedCall(true);
            } else {
                if (!data.value("unread").toBool())
                    event.setIsRead(true);

                event.setFreeText(data.value("text").toString());
            }

            events.append(event);
        }

        eventCatcher.reset();
        if (!events.isEmpty() && !model.addEvents(events)) {
            qWarning() << "Error adding messages for conversation" << groupCount;
            ok = false;
            continue;
        }
        eventCatcher.waitCommit(events.size());

        qDebug() << "CONVERSATION " << group.id() << ":" << group.localUid() << group.recipients().debugString() << "-"
                 << events.size() << "messages";
    }

    if (!ok) {
        qWarning() << "Errors occurred while importing JSON file. Data may be incomplete.";
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
#ifndef QT_NO_EXCEPTIONS
    try {
#endif
        QCoreApplication app(argc, argv);

        optionsWithArguments << "-group" << "-startTime" << "-endTime" << "-n" << "-text" << "-relativeDate";

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
        } else if (args.at(1) == "isvideo" && args.count() > 2) {
            return doIsVideo(args, options);
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
        } else if (args.at(1) == "markallcallsread") {
            return doMarkAllCallsRead(args, options);
        } else if (args.at(1) == "export" && args.count() > 2) {
            return doExport(args, options);
        } else if (args.at(1) == "import") {
            return doImport(args, options);
        } else if (args.at(1) == "import-json" && args.count() >= 3) {
            return doJsonImport(args, options);
        } else {
            printUsage();
        }

        return 0;

#ifndef QT_NO_EXCEPTIONS
    } catch (std::bad_alloc &ba) {
        std::cerr << "Bad alloc exception: " << ba.what() << std::endl;
        return 1;
    }
#endif
}
