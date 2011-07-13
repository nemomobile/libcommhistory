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

#include <QtTest/QtTest>
#include <QDBusConnection>
#include <time.h>
#include "unreadeventsmodeltest.h"
#include "groupmodel.h"
#include "conversationmodel.h"
#include "unreadeventsmodel.h"
#include "event.h"
#include "trackerio.h"
#include "modelwatcher.h"

using namespace CommHistory;

#define NUM_EVENTS 5

ModelWatcher watcher;

typedef QList<Event> EventList;
Group group1;

class UserGroups {
public:
    int groupId;
    uint endTime;
    EventList IMEvents;
    EventList SMSEvents;
    EventList CallEvents;

    UserGroups() { groupId = -1; endTime = 0; }
};

// map by contact
QMap<QString, UserGroups *> user1Groups, user2Groups;

const char *contactIds[] = {
    "user@remotehost",
    "user2@remotehost",
    "test.user@gmail.com"
};
#define NUM_CONTACT_IDS 3

const char *phoneNumbers[] = {
    "123456",
    "+358 (010) 555 1234",
    "666666"
};
#define NUM_PHONE_NUMBERS 3

const char *textContent[] = {
    "The quick brown fox jumps over the lazy dog.",
    "It was a dark and stormy night.",
    "Alussa olivat suo, kuokka ja Jussi."
};

Event createEvent(QString user)
{
    static int prevEndTime = QDateTime::currentDateTime().toTime_t();

    Event e;
    e.setType((qrand() % 3) ? Event::IMEvent : Event::CallEvent);
    e.setType((Event::EventType)((qrand() % 3) + 1));
    e.setDirection((Event::EventDirection)((qrand() & 1) + 1));
    e.setEndTime(QDateTime::fromTime_t(prevEndTime + (qrand() % 3600)));
    e.setStartTime(e.endTime().addSecs(
                       (e.type() == Event::CallEvent ?
                        -(qrand() % 600 + 10) : 0)));
    prevEndTime = e.endTime().toTime_t();
    e.setIsRead(false);
    e.setBytesReceived(qrand() % 1024);
    if (e.type() == Event::SMSEvent) {
        e.setLocalUid("ring/tel/ring");
        e.setRemoteUid(phoneNumbers[qrand() % NUM_PHONE_NUMBERS]);
    } else {
        e.setLocalUid(user);
        e.setRemoteUid(contactIds[qrand() % NUM_CONTACT_IDS]);
    }
    if(e.type() != Event::CallEvent) {
        e.setFreeText(textContent[qrand() % 3]);
    }

    if (e.type() == Event::CallEvent) {
        e.setIsMissedCall( true );
        e.setDirection(Event::Inbound);
    }

    return e;
}

void UnreadEventModelTest::initTestCase()
{
    EventModel model;

    watcher.setLoop(&m_eventLoop);

    qsrand(QDateTime::currentDateTime().toTime_t());
}

void UnreadEventModelTest::addEvent()
{
    UnreadEventsModel model;
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    GroupModel groupModel;
    groupModel.enableContactChanges(false);

    group1.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    QStringList uids;
    uids << "td@localhost";
    group1.setRemoteUids(uids);
    QVERIFY(groupModel.addGroup(group1));

    for (int j = 0; j < NUM_EVENTS; j++) {
        Event e;
        Event dbEvent;
        int id;

        e = createEvent("user@localhost");
        e.setGroupId(group1.id());
        QVERIFY(model.addEvent(e));
        watcher.waitForSignals();
        id = e.id();
        QVERIFY(id != -1);
        QVERIFY(model.trackerIO().getEvent(id, dbEvent));
        QCOMPARE(dbEvent.id(), id);
    }
}


void UnreadEventModelTest::getEvents()
{
    UnreadEventsModel model;
    model.setQueryMode(EventModel::SyncQuery);
    model.getEvents();
    const int count = model.rowCount();
    for(int i = 0; i < count; i++){
        Event event = model.event(model.index(i, 0));
        QVERIFY(event.isRead() == 0);
    }
}

void UnreadEventModelTest::markAsRead()
{
    UnreadEventsModel model;
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    model.getEvents();
    const int count = model.rowCount();

    QList<Event> events;
    for (int i = 0; i < count; i++) {
        Event event = model.event(model.index(count-1-i, 0));
        event.setIsRead(true);
        events << event;
    }
    model.modifyEvents(events);
    if (count) watcher.waitForSignals();
    QVERIFY(model.rowCount() == 0);
}

QTEST_MAIN(UnreadEventModelTest)
