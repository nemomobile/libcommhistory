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
#include "draftmodeltest.h"
#include "draftmodel.h"
#include "event.h"
#include "common.h"

using namespace CommHistory;

Group group1, group2;
QEventLoop *loop;
int eventCounter;

void DraftModelTest::eventsAddedSlot(const QList<CommHistory::Event> &events)
{
    qDebug() << "eventsAdded:" << events.count();
    eventCounter -= events.count();
    if (eventCounter <= 0) {
        loop->exit(0);
    }
}

int DraftModelTest::execLoop(int times)
{
    eventCounter = times;
    return loop->exec();
}

void DraftModelTest::initTestCase()
{
    QSKIP("draftmodel is unsupported and should be removed", SkipAll);

    QVERIFY(QDBusConnection::sessionBus().isConnected());

    deleteAll();

    loop = new QEventLoop(this);

    qsrand(QDateTime::currentDateTime().toTime_t());

    addTestGroups(group1, group2);

    QVERIFY(QDBusConnection::sessionBus().registerObject(
                "/DraftModelTest", this));
    QVERIFY(QDBusConnection::sessionBus().connect(
                QString(), QString(), "com.nokia.commhistory", "eventsAdded",
                this, SLOT(eventsAddedSlot(const QList<CommHistory::Event> &))));

    EventModel model;
    addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT2, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT2, group1.id());

    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT1, group1.id());
    addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2, group1.id());
    addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT2, group1.id());

    addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1);
    addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1);

    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id(),
                 "draft", true);
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT2, group1.id(),
                 "draft 2", true);
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id(),
                 "draft 3", true);

    execLoop(13);
}

void DraftModelTest::getEvents()
{
    DraftModel model;
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents());

    QCOMPARE(model.rowCount(), 3);
    for (int i = 0; i < model.rowCount(); i++)
        QVERIFY(model.event(model.index(i, 0)).isDraft());
}

void DraftModelTest::addEvent()
{
    DraftModel model;
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents());
    int rows = model.rowCount();

    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    execLoop(1);
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    execLoop(1);
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    execLoop(1);
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    execLoop(1);
    QVERIFY(addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    execLoop(1);
    QVERIFY(addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    execLoop(1);
    QVERIFY(addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "added event", false, true) != -1);
    execLoop(1);
    QCOMPARE(model.rowCount(), rows);

    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added draft", true) != -1);
    execLoop(1);
    rows++;
    QCOMPARE(model.rowCount(), rows);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("added draft"));

    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT2,
                         group1.id(), "added another draft", true) != -1);
    execLoop(1);
    rows++;
    QCOMPARE(model.rowCount(), rows);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("added another draft"));
}

void DraftModelTest::cleanupTestCase()
{
//    deleteAll();
}

QTEST_MAIN(DraftModelTest)
