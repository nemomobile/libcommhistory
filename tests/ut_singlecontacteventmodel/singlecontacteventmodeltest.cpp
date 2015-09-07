/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
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

#include "singlecontacteventmodeltest.h"

#include "singlecontacteventmodel.h"
#include "event.h"
#include "common.h"
#include "databaseio.h"

#include <QtTest/QtTest>

Group group1, group2;

void SingleContactEventModelTest::initTestCase()
{
    deleteAll();

    qsrand(QDateTime::currentDateTime().toTime_t());
}

void SingleContactEventModelTest::cleanupTestCase()
{
    deleteAll();
    QTest::qWait(100);
}

void SingleContactEventModelTest::testGetEvents_data()
{
    QTest::addColumn<QString>("localId");
    QTest::addColumn<QString>("remoteId");
    QTest::addColumn<int>("eventType");

    QTest::newRow("im") << "/org/freedesktop/Telepathy/Account/gabble/jabber/good_40localhost0"
            << "abc@localhost"
            << (int)Event::IMEvent;
    QTest::newRow("cell") << RING_ACCOUNT
            << "+42382394"
            << (int)Event::SMSEvent;
}

void SingleContactEventModelTest::testGetEvents()
{
    QFETCH(QString, localId);
    QFETCH(QString, remoteId);
    QFETCH(int, eventType);

    deleteAll();
    QTest::qWait(100);

    addTestGroups(group1, group2);

    SingleContactEventModel model;

    int eventId = addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId, group1.id(),
                               "text", false, false, QDateTime::currentDateTime(), remoteId, false);
    QVERIFY(eventId != -1);

    // Not added to the model
    QCOMPARE(model.rowCount(), 0);

    Event event;
    model.databaseIO().getEvent(eventId, event);
    QCOMPARE(event.contactId(), 0);

    int contactId = addTestContact("Correspondent", remoteId, localId);
    QVERIFY(contactId != -1);
    QVERIFY(addTestContactAddress(contactId, remoteId + "123", localId));

    // We need to wait for libcontacts to process this contact addition, which involves
    // various delays and event handling asynchronicities
    QTest::qWait(1000);

    QVERIFY(model.getEvents(contactId));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 1);

    event = model.event(model.index(0, 0));
    QCOMPARE(event.id(), eventId);
    QCOMPARE(event.contactId(), contactId);
    QCOMPARE(event.contactName(), QString("Correspondent"));

    // Reset to an unused recipient
    QVERIFY(model.getEvents(Recipient(RING_ACCOUNT, "not-a-real-number")));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 0);

    // Look up the original contact via the event UID
    QVERIFY(model.getEvents(Recipient(localId, remoteId)));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 1);

    event = model.event(model.index(0, 0));
    QCOMPARE(event.id(), eventId);
    QCOMPARE(event.contactId(), contactId);
    QCOMPARE(event.contactName(), QString("Correspondent"));

    // Look up the contact via the a different UID
    QVERIFY(model.getEvents(Recipient(localId, remoteId + "123")));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 1);

    event = model.event(model.index(0, 0));
    QCOMPARE(event.id(), eventId);
    QCOMPARE(event.contactId(), contactId);
    QCOMPARE(event.contactName(), QString("Correspondent"));

    // Add a non-matching event
    eventId = addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId, group1.id(),
                           "text", false, false, QDateTime::currentDateTime(), remoteId + "999", false);
    QVERIFY(eventId != -1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.findEvent(eventId), QModelIndex());

    // Add a matching event
    eventId = addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId, group1.id(),
                           "text", false, false, QDateTime::currentDateTime(), remoteId + "123", false);
    QVERIFY(eventId != -1);
    QCOMPARE(model.rowCount(), 2);

    event = model.event(model.findEvent(eventId));
    QCOMPARE(event.id(), eventId);
    QCOMPARE(event.contactId(), contactId);
    QCOMPARE(event.contactName(), QString("Correspondent"));
}

void SingleContactEventModelTest::testLimitOffset_data()
{
    QTest::addColumn<QString>("localId");
    QTest::addColumn<QString>("remoteId");
    QTest::addColumn<int>("eventType");

    QTest::newRow("im") << "/org/freedesktop/Telepathy/Account/gabble/jabber/good_40localhost0"
            << "abc@localhost"
            << (int)Event::IMEvent;
    QTest::newRow("cell") << RING_ACCOUNT
            << "+42382394"
            << (int)Event::SMSEvent;
}

void SingleContactEventModelTest::testLimitOffset()
{
    QFETCH(QString, localId);
    QFETCH(QString, remoteId);
    QFETCH(int, eventType);

    deleteAll();
    QTest::qWait(100);

    addTestGroups(group1, group2);

    SingleContactEventModel model;

    QList<int> eventIds;
    QDateTime when(QDateTime::currentDateTime());
    for (int i = 0; i  < 4; ++i) {
        when = when.addSecs(-1);
        int eventId = addTestEvent(model, (Event::EventType)eventType, Event::Inbound, localId, group1.id(),
                                   "text", false, false, when, remoteId, false);
        QVERIFY(eventId != -1);
        eventIds.append(eventId);
    }

    // Not added to the model
    QCOMPARE(model.rowCount(), 0);

    Event event;

    int contactId = addTestContact("Correspondent", remoteId, localId);
    QVERIFY(contactId != -1);
    QVERIFY(addTestContactAddress(contactId, remoteId + "123", localId));

    // We need to wait for libcontacts to process this contact addition, which involves
    // various delays and event handling asynchronicities
    QTest::qWait(1000);

    QVERIFY(model.getEvents(contactId));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 4);

    QCOMPARE(model.event(model.index(0, 0)).id(), eventIds.at(0));
    QCOMPARE(model.event(model.index(1, 0)).id(), eventIds.at(1));
    QCOMPARE(model.event(model.index(2, 0)).id(), eventIds.at(2));
    QCOMPARE(model.event(model.index(3, 0)).id(), eventIds.at(3));

    model.setLimit(2);
    QVERIFY(model.getEvents(contactId));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 2);

    QCOMPARE(model.event(model.index(0, 0)).id(), eventIds.at(0));
    QCOMPARE(model.event(model.index(1, 0)).id(), eventIds.at(1));

    model.setOffset(2);
    QVERIFY(model.getEvents(contactId));
    QTRY_VERIFY(model.isReady());
    QCOMPARE(model.rowCount(), 2);

    QCOMPARE(model.event(model.index(0, 0)).id(), eventIds.at(2));
    QCOMPARE(model.event(model.index(1, 0)).id(), eventIds.at(3));
}

QTEST_MAIN(SingleContactEventModelTest)
