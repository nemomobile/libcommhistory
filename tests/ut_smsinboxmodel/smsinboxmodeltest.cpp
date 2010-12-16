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
#include "smsinboxmodeltest.h"
#include "smsinboxmodel.h"
#include "event.h"
#include "common.h"
#include "modelwatcher.h"

using namespace CommHistory;

Group group1, group2;
QEventLoop loop;
ModelWatcher watcher;

void SMSInboxModelTest::initTestCase()
{
    QVERIFY(QDBusConnection::sessionBus().isConnected());

    deleteAll();

    watcher.setLoop(&loop);

    qsrand(QDateTime::currentDateTime().toTime_t());

    addTestGroups(group1, group2);

    EventModel model;
    watcher.setModel(&model);
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

    watcher.waitForSignals(11);
}

void SMSInboxModelTest::getEvents()
{
    SMSInboxModel model;
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents());

    QCOMPARE(model.rowCount(), 2);
    for (int i = 0; i < model.rowCount(); i++) {
        QCOMPARE(model.event(model.index(i, 0)).type(), Event::SMSEvent);
        QCOMPARE(model.event(model.index(i, 0)).direction(), Event::Inbound);
    }
}

void SMSInboxModelTest::addEvent()
{
    SMSInboxModel model;
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents());
    int rows = model.rowCount();

    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Inbound, ACCOUNT1,
                         group2.id(), "added event") != -1);
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    QVERIFY(addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    QVERIFY(addTestEvent(model, Event::CallEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added event") != -1);
    QVERIFY(addTestEvent(model, Event::SMSEvent, Event::Inbound, ACCOUNT2,
                         group2.id(), "added event") != -1);
    QVERIFY(addTestEvent(model, Event::CallEvent, Event::Inbound, ACCOUNT1,
                         group1.id(), "added event", false, true) != -1);
    QVERIFY(addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                         group1.id(), "added draft", false, true) != -1);
    watcher.waitForSignals(9);
    QCOMPARE(model.rowCount(), rows + 2);
    QCOMPARE(model.event(model.index(0, 0)).freeText(), QString("added event"));
}

void SMSInboxModelTest::cleanupTestCase()
{
//    deleteAll();
}

QTEST_MAIN(SMSInboxModelTest)
