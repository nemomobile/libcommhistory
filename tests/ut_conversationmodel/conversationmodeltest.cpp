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

#include <QtTest/QtTest>
#include <QDBusConnection>
#include "conversationmodeltest.h"
#include "groupmodel.h"
#include "conversationmodel.h"
#include "adaptor.h"
#include "event.h"
#include "common.h"
#include "trackerio.h"
#include "modelwatcher.h"

using namespace CommHistory;

Group group1, group2;
QEventLoop *loop;

ModelWatcher watcher;

void ConversationModelTest::initTestCase()
{
    QVERIFY(QDBusConnection::sessionBus().isConnected());

    deleteAll();

    loop = new QEventLoop(this);

    watcher.setLoop(loop);

    qsrand(QDateTime::currentDateTime().toTime_t());

    addTestGroups(group1, group2);

    EventModel model;
    watcher.setModel(&model);
    addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id());

    addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT2, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT2, group1.id());

    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT1, group1.id());

    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2, group1.id());
    addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT2, group1.id());

    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1);
    addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1);

    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id(),
                 "draft", true);

    // status message:
    // NOTE: this event is not visible in any of the further tests
    addTestEvent(model, Event::StatusMessageEvent, Event::Outbound, ACCOUNT1,
                 group1.id(), "statue message", false, false,
                 QDateTime::currentDateTime(), QString(), true);

    watcher.waitForSignals(13, 14);
}

void ConversationModelTest::getEvents_data()
{
    QTest::addColumn<bool>("useThread");

    QTest::newRow("Without thread") << false;
    QTest::newRow("Use thread") << true;
}

void ConversationModelTest::getEvents()
{
    QFETCH(bool, useThread);

    QThread modelThread;

    ConversationModel model;
    model.enableContactChanges(false);
    watcher.setModel(&model);

    if (useThread) {
        modelThread.start();
        model.setBackgroundThread(&modelThread);
    }

    QVERIFY(model.getEvents(group1.id()));
    QVERIFY(watcher.waitForModelReady(5000));

    QCOMPARE(model.rowCount(), 10);
    for (int i = 0; i < model.rowCount(); i++)
        QVERIFY(model.event(model.index(i, 0)).type() != Event::CallEvent);

    // add but don't save status message and check content again
    addTestEvent(model, Event::StatusMessageEvent, Event::Outbound, ACCOUNT1,
                 group1.id(), "status message", false, false,
                 QDateTime::currentDateTime(), QString(), true);
    watcher.waitForSignals(-1, 1);
    QCOMPARE(model.rowCount(), 11);
    for (int i = 0; i < model.rowCount(); i++)
        QVERIFY(model.event(model.index(i, 0)).type() != Event::CallEvent);
    // NOTE: since setFilter re-fetches data from tracker, status message event is lost

    /* filtering by type */
    QVERIFY(model.setFilter(Event::IMEvent));
    QVERIFY(watcher.waitForModelReady(5000));
    QCOMPARE(model.rowCount(), 6);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).type(), Event::IMEvent);

    QVERIFY(model.setFilter(Event::SMSEvent));
    QVERIFY(watcher.waitForModelReady(5000));
    QCOMPARE(model.rowCount(), 4);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).type(), Event::SMSEvent);

    /* filtering by account */
    QVERIFY(model.setFilter(Event::UnknownType, ACCOUNT1));
    QVERIFY(watcher.waitForModelReady(5000));
    QCOMPARE(model.rowCount(), 6);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).localUid(), ACCOUNT1);

    QVERIFY(model.setFilter(Event::UnknownType, ACCOUNT2));
    QVERIFY(watcher.waitForModelReady(5000));
    QCOMPARE(model.rowCount(), 4);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).localUid(), ACCOUNT2);

    /* filtering by direction */
    QVERIFY(model.setFilter(Event::UnknownType, QString(), Event::Inbound));
    QVERIFY(watcher.waitForModelReady(5000));
    QCOMPARE(model.rowCount(), 5);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).direction(), Event::Inbound);

    QVERIFY(model.setFilter(Event::UnknownType, QString(), Event::Outbound));
    QVERIFY(watcher.waitForModelReady(5000));
    QCOMPARE(model.rowCount(), 5);
    for (int i = 0; i < model.rowCount(); i++)
        QCOMPARE(model.event(model.index(i, 0)).direction(), Event::Outbound);

    /* mixed filtering */
    QVERIFY(model.setFilter(Event::IMEvent, ACCOUNT1, Event::Outbound));
    QVERIFY(watcher.waitForModelReady(5000));
    QCOMPARE(model.rowCount(), 2);
    for (int i = 0; i < model.rowCount(); i++) {
        QCOMPARE(model.event(model.index(i, 0)).type(), Event::IMEvent);
        QCOMPARE(model.event(model.index(i, 0)).localUid(), ACCOUNT1);
        QCOMPARE(model.event(model.index(i, 0)).direction(), Event::Outbound);
    }

    modelThread.quit();
    modelThread.wait(3000);
}

void ConversationModelTest::addEvent()
{
    ConversationModel model;
    model.enableContactChanges(false);
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));
    int rows = model.rowCount();

    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    watcher.waitForSignals();
    rows++;
    QCOMPARE(model.rowCount(), rows);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("added event"));

    /* filtering by type */
    QVERIFY(model.setFilter(Event::IMEvent));
    rows = model.rowCount();
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "im 1") != -1);
    watcher.waitForSignals();
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "sms 1") != -1);
    watcher.waitForSignals();
    QCOMPARE(model.rowCount(), rows + 1);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("im 1"));

    /* filtering by account */
    QVERIFY(model.setFilter(Event::UnknownType, ACCOUNT1));
    rows = model.rowCount();
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2,
                         group1.id(), "account 2") != -1);
    watcher.waitForSignals();
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "account 1") != -1);
    watcher.waitForSignals();
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "account 1") != -1);
    watcher.waitForSignals();
    QCOMPARE(model.rowCount(), rows + 2);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("account 1"));

    /* filtering by direction */
    QVERIFY(model.setFilter(Event::UnknownType, "", Event::Inbound));
    rows = model.rowCount();
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2,
                         group1.id(), "in") != -1);
    watcher.waitForSignals();
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "out") != -1);
    watcher.waitForSignals();
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "in") != -1);
    watcher.waitForSignals();
    QCOMPARE(model.rowCount(), rows + 2);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("in"));

    /* mixed filtering */
    QVERIFY(model.setFilter(Event::SMSEvent, ACCOUNT2, Event::Inbound));
    rows = model.rowCount();
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT2,
                         group1.id(), "added event") != -1);
    watcher.waitForSignals();
    QCOMPARE(model.rowCount(), rows);
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2,
                         group1.id(), "filtering works") != -1);
    watcher.waitForSignals();
    QCOMPARE(model.rowCount(), rows + 1);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("filtering works"));
}

void ConversationModelTest::modifyEvent()
{
    ConversationModel model;
    model.enableContactChanges(false);
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));

    Event event;
    /* modify invalid event */
    QVERIFY(!model.modifyEvent(event));
    QVERIFY(model.lastError().isValid());

    int row = rand() % model.rowCount();
    event = model.event(model.index(row, 0));
    qDebug() << row << event.id() << event.freeText();
    event.setFreeText("modified event");
    QDateTime modified = event.lastModified();
    QVERIFY(model.modifyEvent(event));
    watcher.waitForSignals();
    QVERIFY(model.trackerIO().getEvent(event.id(), event));
    QCOMPARE(event.freeText(), QString("modified event"));

    QSKIP("Make nie:contentLastUpdated handling consistent", SkipSingle);
    event = model.event(model.index(row, 0));
    QCOMPARE(event.freeText(), QString("modified event"));
    QVERIFY(event.lastModified() > modified);
}

void ConversationModelTest::deleteEvent()
{
    ConversationModel model;
    model.enableContactChanges(false);
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));

    Event event;
    /* delete invalid event */
    QVERIFY(!model.deleteEvent(event));
    QVERIFY(model.lastError().isValid());

    int rows = model.rowCount();
    int row = rand() % rows;
    event = model.event(model.index(row, 0));
    qDebug() << row << event.id();
    QVERIFY(model.deleteEvent(event.id()));
    watcher.waitForSignals();
    QVERIFY(!model.trackerIO().getEvent(event.id(), event));
    QVERIFY(model.lastError().isValid());
    QVERIFY(model.event(model.index(row, 0)).id() != event.id());
    QVERIFY(model.rowCount() == rows - 1);
}

void ConversationModelTest::treeMode()
{
    GroupModel groupModel;
    ConversationModel model;
    model.enableContactChanges(false);
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    model.setTreeMode(true);

    Group group;
    group.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    QStringList uids;
    uids << "td3@localhost";
    group.setRemoteUids(uids);
    QVERIFY(groupModel.addGroup(group));

    QDateTime today = QDateTime::currentDateTime();
    QDateTime yesterday = today.addDays(-1);
    QDateTime lastWeek = today.addDays(-7);
    QDateTime lastMonth = today.addMonths(-1);
    QDateTime older = today.addMonths(-7);

    // add one event for yesterday
    Event event;
    event.setType(Event::IMEvent);
    event.setDirection(Event::Inbound);
    event.setGroupId(group.id());
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRemoteUid("td3@localhost");
    event.setFreeText("yesterday's event");
    event.setEndTime(yesterday);
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();

    qDebug() << group.id();
    QVERIFY(model.getEvents(group.id()));
    QVERIFY(model.hasChildren());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("Yesterday"));
    QModelIndex parent = model.index(0, 0);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("yesterday's event"));

    // ...two for today
    event.setEndTime(today);
    event.setFreeText("today's event");
    QVERIFY(model.addEvent(event));
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals(2);

    QVERIFY(model.getEvents(group.id()));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("Today"));
    QCOMPARE(model.event(model.index(1, 0)).freeText(), QString("Yesterday"));
    parent = model.index(0, 0);
    QCOMPARE(model.rowCount(parent), 2);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("today's event"));
    QCOMPARE(model.event(model.index(1, 0, parent)).freeText(), QString("today's event"));
    parent = model.index(1, 0);
    QCOMPARE(model.rowCount(parent), 1);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("yesterday's event"));

    // ...three for last month
    event.setEndTime(lastMonth);
    event.setFreeText("last month's event");
    QVERIFY(model.addEvent(event));
    QVERIFY(model.addEvent(event));
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals(3);

    QVERIFY(model.getEvents(group.id()));
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("Today"));
    QCOMPARE(model.event(model.index(1, 0)).freeText(), QString("Yesterday"));
    QCOMPARE(model.event(model.index(2, 0)).freeText(), QString("1 months ago"));
    parent = model.index(0, 0);
    QCOMPARE(model.rowCount(parent), 2);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("today's event"));
    QCOMPARE(model.event(model.index(1, 0, parent)).freeText(), QString("today's event"));
    parent = model.index(1, 0);
    QCOMPARE(model.rowCount(parent), 1);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("yesterday's event"));
    parent = model.index(2, 0);
    QCOMPARE(model.rowCount(parent), 3);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("last month's event"));
    QCOMPARE(model.event(model.index(1, 0, parent)).freeText(), QString("last month's event"));
    QCOMPARE(model.event(model.index(2, 0, parent)).freeText(), QString("last month's event"));

    // ...a couple for last week
    event.setEndTime(lastWeek);
    event.setFreeText("last week's event");
    QVERIFY(model.addEvent(event));
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals(2);

    QVERIFY(model.getEvents(group.id()));
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("Today"));
    QCOMPARE(model.event(model.index(1, 0)).freeText(), QString("Yesterday"));
    QCOMPARE(model.event(model.index(2, 0)).freeText(), QString("1 weeks ago"));
    QCOMPARE(model.event(model.index(3, 0)).freeText(), QString("1 months ago"));
    parent = model.index(0, 0);
    QCOMPARE(model.rowCount(parent), 2);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("today's event"));
    QCOMPARE(model.event(model.index(1, 0, parent)).freeText(), QString("today's event"));
    parent = model.index(1, 0);
    QCOMPARE(model.rowCount(parent), 1);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("yesterday's event"));
    parent = model.index(2, 0);
    QCOMPARE(model.rowCount(parent), 2);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("last week's event"));
    QCOMPARE(model.event(model.index(1, 0, parent)).freeText(), QString("last week's event"));
    parent = model.index(3, 0);
    QCOMPARE(model.rowCount(parent), 3);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("last month's event"));
    QCOMPARE(model.event(model.index(1, 0, parent)).freeText(), QString("last month's event"));
    QCOMPARE(model.event(model.index(2, 0, parent)).freeText(), QString("last month's event"));

    // ...and one really old
    event.setEndTime(older);
    event.setFreeText("old event");
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();

    QVERIFY(model.getEvents(group.id()));
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("Today"));
    QCOMPARE(model.event(model.index(1, 0)).freeText(), QString("Yesterday"));
    QCOMPARE(model.event(model.index(2, 0)).freeText(), QString("1 weeks ago"));
    QCOMPARE(model.event(model.index(3, 0)).freeText(), QString("1 months ago"));
    QCOMPARE(model.event(model.index(4, 0)).freeText(), QString("Older"));
    parent = model.index(0, 0);
    QCOMPARE(model.rowCount(parent), 2);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("today's event"));
    QCOMPARE(model.event(model.index(1, 0, parent)).freeText(), QString("today's event"));
    parent = model.index(1, 0);
    QCOMPARE(model.rowCount(parent), 1);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("yesterday's event"));
    parent = model.index(2, 0);
    QCOMPARE(model.rowCount(parent), 2);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("last week's event"));
    QCOMPARE(model.event(model.index(1, 0, parent)).freeText(), QString("last week's event"));
    parent = model.index(3, 0);
    QCOMPARE(model.rowCount(parent), 3);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("last month's event"));
    QCOMPARE(model.event(model.index(1, 0, parent)).freeText(), QString("last month's event"));
    QCOMPARE(model.event(model.index(2, 0, parent)).freeText(), QString("last month's event"));
    parent = model.index(4, 0);
    QCOMPARE(model.rowCount(parent), 1);
    QCOMPARE(model.event(model.index(0, 0, parent)).freeText(), QString("old event"));
}

void ConversationModelTest::asyncMode()
{
    ConversationModel model;
    model.enableContactChanges(false);
    watcher.setModel(&model);
    connect(&model, SIGNAL(modelReady()), this, SLOT(modelReadySlot()));
    QVERIFY(model.getEvents(group1.id()));
    QVERIFY(watcher.waitForModelReady(5000));
}

void ConversationModelTest::sorting()
{
    EventModel model;
    model.setQueryMode(EventModel::StreamedAsyncQuery);
    model.setFirstChunkSize(5);
    model.enableContactChanges(false);
    watcher.setModel(&model);

    //add events with the same timestamp
    QDateTime now = QDateTime::currentDateTime();
    QDateTime future = now.addSecs(10);

    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "I", false, false, now);
    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "II", false, false, now);
    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "III", false, false, future);
    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "IV", false, false, future);
    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                 group1.id(), "V", false, false, future);

    watcher.waitForSignals(5, 5);

    ConversationModel conv;
    conv.setQueryMode(EventModel::StreamedAsyncQuery);
    conv.setFirstChunkSize(5);
    conv.enableContactChanges(false);
    QSignalSpy rowsInserted(&conv, SIGNAL(rowsInserted(const QModelIndex &, int, int)));

    QVERIFY(conv.getEvents(group1.id()));

    QVERIFY(waitSignal(rowsInserted, 5000));

    QVERIFY(conv.rowCount() >= 5 );

    QCOMPARE(conv.event(conv.index(0, 0)).freeText(), QLatin1String("V"));
    QCOMPARE(conv.event(conv.index(1, 0)).freeText(), QLatin1String("IV"));
    QCOMPARE(conv.event(conv.index(2, 0)).freeText(), QLatin1String("III"));
    QCOMPARE(conv.event(conv.index(3, 0)).freeText(), QLatin1String("II"));
    QCOMPARE(conv.event(conv.index(4, 0)).freeText(), QLatin1String("I"));
}


void ConversationModelTest::cleanupTestCase()
{
//    deleteAll();
}

QTEST_MAIN(ConversationModelTest)
