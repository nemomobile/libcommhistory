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
#include <QtTracker/Tracker>

#include <time.h>
#include "eventmodeltest.h"
#include "eventmodel.h"
#include "conversationmodel.h"
#include "groupmodel.h"
#include "adaptor.h"
#include "event.h"
#include "common.h"
#include "trackerio.h"

#include "modelwatcher.h"

using namespace CommHistory;

Group group1, group2;
Event im, sms, call;
QEventLoop loop;

int groupUpdated = 0;
int groupDeleted = 0;

ModelWatcher watcher;

void EventModelTest::groupsUpdatedSlot(const QList<int> &groupIds)
{
    qDebug() << Q_FUNC_INFO << groupIds;
    if (!groupIds.isEmpty())
        groupUpdated = groupIds.first();
}

void EventModelTest::groupsDeletedSlot(const QList<int> &groupIds)
{
        qDebug() << Q_FUNC_INFO << groupIds;
    if (!groupIds.isEmpty())
        groupDeleted = groupIds.first();
}

void EventModelTest::initTestCase()
{
    deleteAll();

    qsrand(QDateTime::currentDateTime().toTime_t());

    new Adaptor(this);
    QVERIFY(QDBusConnection::sessionBus().registerObject(
                "/EventModelTest", this));

    watcher.setLoop(&loop);

    QDBusConnection::sessionBus().connect(
        QString(), QString(), "com.nokia.commhistory", "groupsUpdated",
        this, SLOT(groupsUpdatedSlot(const QList<int> &)));
    QDBusConnection::sessionBus().connect(
        QString(), QString(), "com.nokia.commhistory", "groupsDeleted",
        this, SLOT(groupsDeletedSlot(const QList<int> &)));

    addTestGroups(group1, group2);
}

void EventModelTest::testMessageToken()
{
    EventModel model;
    watcher.setModel(&model);

    sms.setGroupId(group1.id());
    sms.setType(Event::SMSEvent);
    sms.setDirection(Event::Outbound);
    sms.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    sms.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    sms.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    sms.setRemoteUid("123456");
    sms.setFreeText("smstest");
    sms.setMessageToken("1234567890");
    QVERIFY(model.addEvent(sms));
    watcher.waitForSignals();
    QCOMPARE(watcher.addedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(sms.id() != -1);
    Event event;
    QVERIFY(model.trackerIO().getEventByMessageToken(sms.messageToken(), event));
    QVERIFY(compareEvents(event, sms));
}

void EventModelTest::testAddEvent()
{
    EventModel model;
    watcher.setModel(&model);

    /* add invalid event */
    QVERIFY(!model.addEvent(im));
    QVERIFY(model.lastError().isValid());
    im.setType(Event::IMEvent);
    /* missing direction, group id */
    QVERIFY(!model.addEvent(im));
    QVERIFY(model.lastError().isValid());
    im.setDirection(Event::Outbound);
    /* missing group id */
    QVERIFY(!model.addEvent(im));
    QVERIFY(model.lastError().isValid());

    /* add valid IM, SMS and call */
    im.setGroupId(group1.id());
    im.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    im.setRemoteUid("td@localhost");
    im.setFreeText("imtest");
    QVERIFY(model.addEvent(im));
    watcher.waitForSignals();
    QVERIFY(im.id() != -1);
    QCOMPARE(watcher.addedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(compareEvents(watcher.lastAdded()[0], im));

    // TODO: sync with tracker?
    Event event;
    QVERIFY(model.trackerIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(event, im));

    sms.setGroupId(group1.id());
    sms.setType(Event::SMSEvent);
    sms.setDirection(Event::Outbound);
    sms.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    sms.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    sms.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    sms.setRemoteUid("123456");
    sms.setFreeText("smstest");
    QVERIFY(model.addEvent(sms));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(sms.id() != -1);
    QCOMPARE(watcher.addedCount(), 1);
    QVERIFY(compareEvents(watcher.lastAdded()[0], sms));

    QVERIFY(model.trackerIO().getEvent(sms.id(), event));
    QVERIFY(compareEvents(event, sms));

    call.setType(Event::CallEvent);
    call.setDirection(Event::Outbound);
    call.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    call.setEndTime(QDateTime::fromString("2009-08-26T09:42:47Z", Qt::ISODate));
    call.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    call.setRemoteUid("td@localhost");
    call.setIsRead(true);
    QVERIFY(model.addEvent(call));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(call.id() != -1);
    QCOMPARE(watcher.addedCount(), 1);
    QVERIFY(compareEvents(watcher.lastAdded()[0], call));

    QVERIFY(model.trackerIO().getEvent(call.id(), event));
    QVERIFY(compareEvents(event, call));

    // test setting non-existent group id
#if 0 // it's broken now because insert request is split in several and only one of them
      // has check for existent group
    sms.setGroupId(group1.id() + 9999999);
    QVERIFY(model.addEvent(sms));
    watcher.waitForSignals();
    QVERIFY(!model.trackerIO().getEvent(sms.id(), event));
#endif
}

void EventModelTest::testAddEvents()
{
    EventModel model;
    watcher.setModel(&model);

    QList<Event> events;
    Event e1, e2, e3;
    e1.setGroupId(group1.id());
    e1.setType(Event::IMEvent);
    e1.setDirection(Event::Outbound);
    e1.setStartTime(QDateTime::fromString("2010-01-08T13:37:00Z", Qt::ISODate));
    e1.setEndTime(QDateTime::fromString("2010-01-08T13:37:00Z", Qt::ISODate));
    e1.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    e1.setRemoteUid("td@localhost");
    e1.setFreeText("addEvents 1");

    e2.setGroupId(group1.id());
    e2.setType(Event::IMEvent);
    e2.setDirection(Event::Inbound);
    e2.setStartTime(QDateTime::fromString("2010-01-08T13:37:10Z", Qt::ISODate));
    e2.setEndTime(QDateTime::fromString("2010-01-08T13:37:10Z", Qt::ISODate));
    e2.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    e2.setRemoteUid("td@localhost");
    e2.setFreeText("addEvents 2");

    events << e1 << e2;
    QVERIFY(model.addEvents(events));
    watcher.waitForSignals();
    QCOMPARE(watcher.addedCount(), 2);
    QCOMPARE(watcher.committedCount(), 2);
    QVERIFY(compareEvents(watcher.lastAdded()[0], e1));
    QVERIFY(compareEvents(watcher.lastAdded()[1], e2));

    e3.setGroupId(group1.id());
    e3.setType(Event::IMEvent);
    e3.setDirection(Event::Inbound);
    e3.setStartTime(QDateTime::fromString("2010-01-08T13:37:20Z", Qt::ISODate));
    e3.setEndTime(QDateTime::fromString("2010-01-08T13:37:20Z", Qt::ISODate));
    e3.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    e3.setRemoteUid("td@localhost");
    e3.setFreeText("addEvents 3");

    events.clear();
    events << e3;
    QVERIFY(model.addEvents(events,true)); // Add to model only, not into tracker.
    watcher.waitForSignals(-1); // -1 -> Do not wait for committed signal because we do not store to tracker.
    QCOMPARE(watcher.addedCount(), 1);
    QCOMPARE(watcher.committedCount(), 0);
    QVERIFY(compareEvents(watcher.lastAdded()[0], e3));
}

void EventModelTest::testModifyEvent()
{
    EventModel model;
    watcher.setModel(&model);

    im.setType(Event::IMEvent);
    im.setDirection(Event::Outbound);
    im.setGroupId(group1.id());
    im.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    im.setRemoteUid("td@localhost");
    im.setFreeText("imtest");
    QVERIFY(model.addEvent(im));
    watcher.waitForSignals();
    QVERIFY(im.id() != -1);
    QCOMPARE(watcher.addedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(compareEvents(watcher.lastAdded()[0], im));

    Event event;
    QVERIFY(!model.modifyEvent(event));
    QVERIFY(model.lastError().isValid());
    QCOMPARE(watcher.updatedCount(), 0);

    im.resetModifiedProperties();
    im.setFreeText("imtest modified");
    im.setStartTime(QDateTime::currentDateTime());
    im.setEndTime(QDateTime::currentDateTime());
    im.setIsRead(false);
    // should we actually test more properties?
    QVERIFY(model.modifyEvent(im));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QCOMPARE(watcher.updatedCount(), 1);
    QVERIFY(compareEvents(watcher.lastUpdated()[0], im));

    QVERIFY(model.trackerIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(im, event));

    // test mark read case
    im.resetModifiedProperties();
    im.setIsRead(true);
    QVERIFY(model.modifyEvent(im));
    watcher.waitForSignals();
    QCOMPARE(watcher.updatedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(compareEvents(watcher.lastUpdated()[0], im));

    QVERIFY(model.trackerIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(im, event));

    // test derlivery report case
    im.resetModifiedProperties();
    im.setDirection(Event::Outbound);
    im.setStatus(Event::SentStatus);
    im.setStartTime(QDateTime::currentDateTime());

    QVERIFY(model.modifyEvent(im));
    watcher.waitForSignals();
    QCOMPARE(watcher.updatedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(compareEvents(watcher.lastUpdated()[0], im));

    QVERIFY(model.trackerIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(im, event));

    im.setStatus(CommHistory::Event::DeliveredStatus);
    im.setEndTime(QDateTime::currentDateTime());

    QVERIFY(model.modifyEvent(im));
    watcher.waitForSignals();
    QCOMPARE(watcher.updatedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(compareEvents(watcher.lastUpdated()[0], im));

    QVERIFY(model.trackerIO().getEvent(im.id(), event));
    QVERIFY(compareEvents(im, event));

    int imId = im.id();
    im.setId(imId + 999);
    QVERIFY(model.modifyEvent(im));
    watcher.waitForSignals();
    QVERIFY(model.lastError().isValid());
    QCOMPARE(watcher.updatedCount(), 0);
    QCOMPARE(watcher.committedCount(), 0);
    //
    im.setId(imId);

    // call properties
    call.resetModifiedProperties();
    call.setIsMissedCall(true);
    call.setIsEmergencyCall(true);

    QVERIFY(model.modifyEvent(call));
    watcher.waitForSignals();
    QCOMPARE(watcher.updatedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(compareEvents(watcher.lastUpdated()[0], call));

    QVERIFY(model.trackerIO().getEvent(call.id(), event));

    QVERIFY(compareEvents(call, event));
}

void EventModelTest::testDeleteEvent()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    QVERIFY(!model.deleteEvent(event));
    QVERIFY(model.lastError().isValid());

    event.setType(Event::IMEvent);
    event.setDirection(Event::Inbound);
    event.setGroupId(group1.id());
    event.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRemoteUid("td@localhost");
    event.setFreeText("deletetest");
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QVERIFY(event.id() != -1);

    QVERIFY(model.deleteEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.deletedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QCOMPARE(watcher.lastDeletedId(), event.id());
    QVERIFY(!model.trackerIO().getEvent(event.id(), event));

    event.setType(Event::SMSEvent);
    event.setFreeText("deletetest sms");
    event.setRemoteUid("555123456");
    event.setGroupId(group2.id()); //group1 is gone, cause last event in it gone
    event.setId(-1);
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);
    QVERIFY(model.deleteEvent(event));

    watcher.waitForSignals();
    QCOMPARE(watcher.deletedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QCOMPARE(watcher.lastDeletedId(), event.id());
    QVERIFY(!model.trackerIO().getEvent(event.id(), event));
}

void EventModelTest::testDeleteEventGroupUpdated()
{
    EventModel model;
    watcher.setModel(&model);

    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    Group group;
    const QString LOCAL_ID("/org/freedesktop/Telepathy/Account/ring/tel/ring");
    const QString REMOTE_ID("12345");
    addTestGroup(group, LOCAL_ID, REMOTE_ID);

    QVERIFY(groupModel.trackerIO().getGroup(group.id(), group));
    int total = group.totalMessages();
    QCOMPARE(total, 0);

    groupUpdated = -1;
    groupDeleted = -1;

    Event event1;
    event1.setType(Event::SMSEvent);
    event1.setDirection(Event::Inbound);
    event1.setGroupId(group.id());
    event1.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event1.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event1.setLocalUid(LOCAL_ID);
    event1.setRemoteUid(REMOTE_ID);
    event1.setFreeText("deletetest1");
    QVERIFY(model.addEvent(event1));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event1.id() != -1);

    QVERIFY(groupModel.trackerIO().getGroup(group.id(), group));
    total = group.totalMessages();
    QCOMPARE(total, 1);

    Event event2(event1);
    event2.setId(-1);
    event2.setFreeText("deletetest2");
    QVERIFY(model.addEvent(event2));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event2.id() != -1);

    QVERIFY(groupModel.trackerIO().getGroup(group.id(), group));
    total = group.totalMessages();
    QCOMPARE(total, 2);

    QVERIFY(model.deleteEvent(event1.id()));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QCOMPARE(watcher.lastDeletedId(), event1.id());
    QCOMPARE(groupUpdated, group.id());

    QVERIFY(groupModel.trackerIO().getGroup(group.id(), group));

    QVERIFY(model.deleteEvent(event2.id()));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QCOMPARE(watcher.lastDeletedId(), event2.id());
    QCOMPARE(groupDeleted, group.id());

    QVERIFY(!groupModel.trackerIO().getGroup(group.id(), group));

    addTestGroup(group, LOCAL_ID, REMOTE_ID);

    QVERIFY(groupModel.trackerIO().getGroup(group.id(), group));
    total = group.totalMessages();
    QCOMPARE(total, 0);

    event1.setId(-1);
    event1.setGroupId(group.id());
    QVERIFY(model.addEvent(event1));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event1.id() != -1);

    QVERIFY(groupModel.trackerIO().getGroup(group.id(), group));
    total = group.totalMessages();
    QCOMPARE(total, 1);

    QVERIFY(model.deleteEvent(event1));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QCOMPARE(watcher.lastDeletedId(), event1.id());

    QVERIFY(!groupModel.trackerIO().getGroup(group.id(), group));
}

void EventModelTest::testVCard()
{
    QString vcardFilename1( "filename.vcd" );
    QString vcardFilename2( "filename.txt" );
    QString vcardLabel( "Test Label" );

    EventModel model;
    watcher.setModel(&model);

    Event event;
    QVERIFY( !model.deleteEvent( event ) );
    QVERIFY( model.lastError().isValid() );

    // create test data
    event.setType( Event::SMSEvent );
    event.setDirection( Event::Inbound );
    event.setGroupId( group1.id() );
    event.setStartTime( QDateTime::currentDateTime() );
    event.setEndTime( QDateTime::currentDateTime() );
    event.setLocalUid( "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0" );
    event.setRemoteUid( "555123456" );
    event.setFreeText( "vcard test" );
    event.setFromVCard( vcardFilename1, vcardLabel );

    // add event with vcard
    QVERIFY( model.addEvent( event ) );
    // test is event was added successfully
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY( event.id() != -1 );

    // fetch event to check vcard data
    {
        Event test_event;
        QVERIFY( !test_event.isValid() );
        QVERIFY( model.trackerIO().getEvent( event.id(), test_event ) );

        QVERIFY( test_event.isValid() );
        QVERIFY( compareEvents( event, test_event ) );
    }

    // modify data
    event.setFromVCard( vcardFilename2 );
    QVERIFY( model.modifyEvent( event ) );
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);

    // fetch event to check vcard data
    {
        Event test_event;
        QVERIFY( !test_event.isValid() );
        QVERIFY( model.trackerIO().getEvent( event.id(), test_event ) );

        QVERIFY( test_event.isValid() );
        QVERIFY( compareEvents( event, test_event ) );
    }

    QVERIFY( model.deleteEvent( event ) );

    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(!model.trackerIO().getEvent(event.id(), event));
}

void EventModelTest::testDeliveryStatus()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Outbound);
    event.setGroupId(group1.id());
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRemoteUid("55590210");
    event.setFreeText("delivery status test");
    event.setStatus(Event::SendingStatus);
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);

    Event e;
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::SendingStatus);

    event.setStatus(Event::SentStatus);
    QVERIFY(model.modifyEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::SentStatus);

    event.setStatus(Event::DeliveredStatus);
    QVERIFY(model.modifyEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::DeliveredStatus);

    event.setStatus(Event::TemporarilyFailedStatus);
    QVERIFY(model.modifyEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::TemporarilyFailedStatus);

    event.setStatus(Event::PermanentlyFailedStatus);
    QVERIFY(model.modifyEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::PermanentlyFailedStatus);

    event.setStatus(Event::SendingStatus);
    QVERIFY(model.modifyEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(e.status() == Event::SendingStatus);
}

void EventModelTest::testReportDelivery()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Outbound);
    event.setGroupId(group1.id());
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRemoteUid("555888999");
    event.setFreeText("report delivery test");
    event.setReportDelivery(true);

    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);

    Event e;
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(e.reportDelivery());

    event.setReportDelivery(false);

    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);

    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(!e.reportDelivery());
}

void EventModelTest::testMessageParts()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    event.setLocalUid("/org/freedesktop/Telepathy/Account/ring/tel/ring");
    event.setRemoteUid("0506661234");
    event.setType(Event::MMSEvent);
    event.setDirection(Event::Outbound);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setFreeText("mms");
    event.setGroupId(group1.id());

    QList<MessagePart> parts;

    MessagePart part1;
    part1.setContentType("application/smil");
    part1.setPlainTextContent("<smil>blah</smil>");
    MessagePart part2;
    part2.setContentId("text_slide1");
    part2.setContentType("text/plain");
    part2.setPlainTextContent("Here is a photo of my cat. Isn't it cute?");
    MessagePart part3;
    part3.setContentId("catphoto");
    part3.setContentType("image/jpeg");
    part3.setContentSize(101000);
    part3.setContentLocation("/home/user/.mms/msgid001/catphoto.jpg");
    MessagePart part4;
    part4.setContentId("text_slide2");
    part4.setContentType("text/plain");
    part4.setPlainTextContent("And here is a photo of my dog. Isn't it ugly?");
    MessagePart part5;
    part5.setContentId("dogphoto");
    part5.setContentType("image/jpeg");
    part5.setContentSize(202000);
    part5.setContentLocation("/home/user/.mms/msgid001/dogphoto.jpg");

    parts << part1 << part2 << part3 << part4 << part5;
    event.setMessageParts(parts);

    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);

    Event e;
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));
    QCOMPARE(e.messageParts().size(), parts.size());
    foreach (MessagePart part, e.messageParts())
        QVERIFY(parts.indexOf(part) >= 0);
}

void EventModelTest::testMessagePartsQuery_data()
{
    QTest::addColumn<bool>("useThread");

    QTest::newRow("Without thread") << false;
    QTest::newRow("Use thread") << true;
}

void EventModelTest::testMessagePartsQuery()
{
    QFETCH(bool, useThread);
    QString threadPrefix;
    if (useThread) threadPrefix = "thread_";

    EventModel model;
    watcher.setModel(&model);

    Group group;
    const QString LOCAL_ID("/org/freedesktop/Telepathy/Account/ring/tel/ring");
    const QString REMOTE_ID("12345789");
    const QString ATT_PATH("/tmp/.mms/msgid001/");
    addTestGroup(group, LOCAL_ID, REMOTE_ID);

    QString prevDir = QDir::currentPath();
    QDir currentDir = QDir::current();

    QVERIFY(currentDir.mkpath(ATT_PATH));

    Event event;
    event.setLocalUid(LOCAL_ID);
    event.setRemoteUid(REMOTE_ID);
    event.setType(Event::MMSEvent);
    event.setDirection(Event::Outbound);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setFreeText("mms1");
    event.setGroupId(group.id());

    MessagePart part1;
    part1.setContentType("application/smil");
    part1.setPlainTextContent("<smil>blah</smil>");
    MessagePart part2;
    part2.setContentId("text_slide1");
    part2.setContentType("text/plain");
    part2.setPlainTextContent("Here is a photo of my cat. Isn't it cute?");
    MessagePart part3;
    part3.setContentId("catphoto");
    part3.setContentType("image/jpeg");
    part3.setContentSize(101000);
    QString fileName = threadPrefix + "catphoto2.jpg";
    part3.setContentLocation(ATT_PATH + fileName);

#define CREATE_FILE(messageToken, filename) {\
        QString mmsPath = QString("%1/.mms/msg/%2").arg(QDir::homePath()).arg(messageToken); \
        QDir mmsDir(mmsPath); \
        QVERIFY(mmsDir.mkpath(mmsPath)); \
        QDir::setCurrent(mmsDir.path()); \
        qDebug() << "create file" << filename << "in" << mmsDir.path(); \
        QFile photo((filename)); \
        photo.open(QIODevice::WriteOnly); \
        photo.close(); \
        QVERIFY(QFile::exists(photo.fileName()));}

    CREATE_FILE("MSGTOKEN1", fileName)

    QList<MessagePart> parts1;
    parts1 << part1 << part2 << part3;
    event.setMessageParts(parts1);
    event.setMessageToken("MSGTOKEN1");
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);

    event.setId(-1);
    event.setFreeText("mms2");
    MessagePart part4;
    part4.setContentId("text_slide2");
    part4.setContentType("text/plain");
    part4.setPlainTextContent("And here is a photo of my dog. Isn't it ugly?");
    MessagePart part5;
    part5.setContentId("dogphoto2");
    part5.setContentType("image/jpeg");
    part5.setContentSize(202000);
    fileName = threadPrefix + "dogphoto2.jpg";
    part5.setContentLocation(ATT_PATH + fileName);

    CREATE_FILE("MSGTOKEN2", fileName);

    QList<MessagePart> parts2;
    parts2 << part4 << part5;
    event.setMessageParts(parts2);
    event.setMessageToken("MSGTOKEN2");

    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);

    event.setId(-1);
    event.setFreeText("mms3");
    MessagePart part6;
    part6.setContentId("dogphoto3");
    part6.setContentType("image/jpeg");
    part6.setContentSize(203000);
    fileName = threadPrefix + "dogphoto3.jpg";
    part6.setContentLocation(ATT_PATH + fileName);

    CREATE_FILE("MSGTOKEN3", fileName)

    QList<MessagePart> parts3;
    parts3 << part6;
    event.setMessageParts(parts3);
    event.setMessageToken("MSGTOKEN3");

    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);

    QDir::setCurrent(prevDir);

    ModelWatcher convWatcher(&loop);
    ConversationModel convModel;
    convWatcher.setModel(&convModel);

    QSignalSpy modelReady(&convModel, SIGNAL(modelReady()));
    QThread modelThread;

    if (useThread) {
        modelThread.start();
        convModel.setBackgroundThread(&modelThread);
    }

    QVERIFY(convModel.getEvents(group.id()));

    QVERIFY(waitSignal(modelReady, 5000));

    QList<int> eventIds;
    for (int i = 0; i < convModel.rowCount(); i++) {
        Event e = convModel.event(convModel.index(i, 0));

        QVERIFY(e.id() != -1);
        eventIds << e.id();
        qDebug() << e.id() << e.freeText();
        if (e.freeText() == "mms1") {
            QCOMPARE(e.messageParts().size(), parts1.size());
            foreach (MessagePart part, e.messageParts())
                QVERIFY(parts1.indexOf(part) >= 0);
        } else if (e.freeText() == "mms2") {
            QCOMPARE(e.messageParts().size(), parts2.size());
            foreach (MessagePart part, e.messageParts())
                QVERIFY(parts2.indexOf(part) >= 0);
        } else if (e.freeText() == "mms3") {
            QCOMPARE(e.messageParts().size(), parts3.size());
            foreach (MessagePart part, e.messageParts())
                QVERIFY(parts3.indexOf(part) >= 0);
        } else {
            QFAIL("Unexpected message");
        }
    }

    // check mms deleter
    foreach (int eventId, eventIds) {
        qDebug() << "DELETE" << eventId;
        convModel.deleteEvent(eventId);
        convWatcher.waitForSignals();
        QCOMPARE(convWatcher.committedCount(), 1);
        QCOMPARE(convWatcher.deletedCount(), 1);
    }

    qDebug() << "wait thread";
    modelThread.quit();
    modelThread.wait(3000);
    qDebug() << "done";

    QString mmsPath = QString("%1/.mms/msg/").arg(QDir::homePath());
    QVERIFY(!QDir(mmsPath + "MSGTOKEN1").exists());
    QVERIFY(!QDir(mmsPath + "MSGTOKEN2").exists());
    QVERIFY(!QDir(mmsPath + "MSGTOKEN3").exists());
}

void EventModelTest::testCcBcc()
{
    EventModel model;
    watcher.setModel(&model);

    Event event;
    event.setLocalUid("/org/freedesktop/Telepathy/Account/ring/tel/ring");
    event.setRemoteUid("0506661234");
    event.setType(Event::MMSEvent);
    event.setDirection(Event::Outbound);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setFreeText("mms");
    event.setGroupId(group1.id());

    QStringList ccList;
    ccList << "+12345" << "98765" << "555666";
    event.setCcList(ccList);

    QStringList bccList;
    bccList << "+777888" << "999888" << "333555";
    event.setBccList(bccList);

    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);

    Event e;
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));
    QCOMPARE(e.ccList().toSet(), ccList.toSet());
    QCOMPARE(e.bccList().toSet(), bccList.toSet());

    event.resetModifiedProperties();
    ccList.clear();
    ccList << "112" << "358" << "134";
    event.setCcList(ccList);

    bccList.clear();
    bccList << "314" << "15" << "16";
    event.setBccList(bccList);

    QVERIFY(model.modifyEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(event.id() != -1);

    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));
    QCOMPARE(e.ccList().toSet(), ccList.toSet());
    QCOMPARE(e.bccList().toSet(), bccList.toSet());
}


void EventModelTest::testFindEvent()
{
    ConversationModel model;
    Event event;

    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));
    QModelIndex index = model.findEvent(im.id());
    QVERIFY(index.isValid());
    event = index.data(Qt::UserRole).value<Event>();
    QVERIFY(compareEvents(event, im));

    // Test also EventModel's data-method for majority and most important event fields:
    int row = index.row();
    QModelIndex index2 = model.index(row,EventModel::EventId);
    QVariant var2 = model.data(index2);
    QVariant header2 = model.headerData(EventModel::EventId,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("id"));
    int eventId = var2.toInt();
    QCOMPARE(eventId,im.id());

    index2 = model.index(row,EventModel::EventType);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::EventType,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("event_type"));
    int eventType = var2.toInt();
    QCOMPARE(eventType,(int)im.type());

    index2 = model.index(row,EventModel::Direction);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::Direction,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("direction"));
    int direction = var2.toInt();
    QCOMPARE(direction,(int)im.direction());

    index2 = model.index(row,EventModel::IsRead);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::IsRead,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("is_read"));
    bool isRead = var2.toBool();
    QCOMPARE(isRead,im.isRead());

    index2 = model.index(row,EventModel::Status);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::Status,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("status"));
    int status = var2.toInt();
    QCOMPARE(status,(int)im.status());

    index2 = model.index(row,EventModel::LocalUid);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::LocalUid,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("local_uid"));
    QString localUid = var2.toString();
    QCOMPARE(localUid,im.localUid());

    index2 = model.index(row,EventModel::RemoteUid);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::RemoteUid,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("remote_uid"));
    QString remoteUid = var2.toString();
    QCOMPARE(remoteUid,im.remoteUid());

    index2 = model.index(row,EventModel::FreeText);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::FreeText,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("free_text"));
    QString freeText = var2.toString();
    QCOMPARE(freeText,im.freeText());

    index2 = model.index(row,EventModel::GroupId);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::GroupId,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("group_id"));
    int groupId = var2.toInt();
    QCOMPARE(groupId,im.groupId());

    index2 = model.index(row,EventModel::MessageToken);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::MessageToken,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("message_token"));
    QString messageToken = var2.toString();
    QCOMPARE(messageToken,im.messageToken());

    index2 = model.index(row,EventModel::ContactId);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::ContactId,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("contact_id"));
    int contactId = var2.toInt();
    QCOMPARE(contactId,im.contactId());

    index2 = model.index(row,EventModel::StartTime);
    var2 = model.data(index2);
    header2 = model.headerData(EventModel::StartTime,Qt::Horizontal,Qt::DisplayRole);
    QCOMPARE(header2.toString(),QString("start_time"));
    QDateTime startTime = var2.toDateTime();
    QCOMPARE(startTime.toTime_t(),im.startTime().toTime_t());


    index = model.findEvent(-1);
    QVERIFY(!index.isValid());
}

void EventModelTest::testMoveEvent()
{
    addTestGroup(group2,
                 "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0",
                 "td2@localhost");

    groupDeleted = -1;

    EventModel model;
    watcher.setModel(&model);
    Event event;
    event.setGroupId(group1.id());
    event.setType(Event::IMEvent);
    event.setDirection(Event::Inbound);
    event.setStartTime(QDateTime::fromString("2010-01-08T13:39:00Z", Qt::ISODate));
    event.setEndTime(QDateTime::fromString("2010-01-08T13:39:00Z", Qt::ISODate));
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRemoteUid("td@localhost");
    event.setFreeText("moveEvent 1");

    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QCOMPARE(watcher.addedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QVERIFY(compareEvents(watcher.lastAdded()[0], event));

    QVERIFY(model.moveEvent(event,group2.id()));

    watcher.waitForSignals();
    QCOMPARE(watcher.deletedCount(), 1);
    QCOMPARE(watcher.lastDeletedId(),event.id());
    QCOMPARE(watcher.addedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);

    Event eventFromTracker;
    QVERIFY(model.trackerIO().getEvent(event.id(), eventFromTracker));
    QCOMPARE(eventFromTracker.groupId(),group2.id());

    // Try with invalid event:
    Event invalidEvent;
    QVERIFY(!model.moveEvent(invalidEvent,group2.id()));

    // Try to move event to the same group where event already is:
    QVERIFY(model.moveEvent(eventFromTracker,group2.id()));

    // Moving event from a group that has this event as the only one.
    // Then the empty group should be deleted:
    QVERIFY(model.moveEvent(eventFromTracker,group1.id()));

    watcher.waitForSignals();
    QCOMPARE(watcher.deletedCount(), 1);
    QCOMPARE(watcher.lastDeletedId(),eventFromTracker.id());
    QCOMPARE(watcher.addedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);
    QCOMPARE(groupDeleted,group2.id());
}

void EventModelTest::testStreaming_data()
{
    QTest::addColumn<bool>("useThread");

    QTest::newRow("Without thread") << false;
    QTest::newRow("Use thread") << true;
}

void EventModelTest::testStreaming()
{
    //::tracker()->setVerbosity(5);
    QFETCH(bool, useThread);

    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    Group group;

    QThread modelThread;

    QVERIFY(groupModel.trackerIO().getGroup(group1.id(), group));
    int total = group.totalMessages();

    qDebug() << "total msgs: " << total;

    ConversationModel model;
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents(group1.id()));

    ConversationModel streamModel;

    if (useThread) {
        modelThread.start();
        streamModel.setBackgroundThread(&modelThread);
    }

    streamModel.setQueryMode(EventModel::StreamedAsyncQuery);
    const int normalChunkSize = 4;
    const int firstChunkSize = 2;
    streamModel.setChunkSize(normalChunkSize);
    streamModel.setFirstChunkSize(firstChunkSize);
    qRegisterMetaType<QModelIndex>("QModelIndex");
    QSignalSpy rowsInserted(&streamModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)));
    QSignalSpy modelReady(&streamModel, SIGNAL(modelReady()));
    QVERIFY(streamModel.getEvents(group1.id()));

    int count = 0;
    int chunkSize = firstChunkSize;
    while (count < total) {
        qDebug() << "count: " << count;
        if (count > 0)
            chunkSize = normalChunkSize;

        qDebug() << "chunk size: " << chunkSize;

        QVERIFY(waitSignal(rowsInserted, 5000));

        QList<QVariant> args = rowsInserted.takeFirst();
        int expectedEnd = count + chunkSize > total ? total - 1: count + chunkSize - 1;
        qDebug() << "rows start in streaming model: " << args.at(1).toInt();
        qDebug() << "rows start should be: " << count;
        qDebug() << "rows end in streaming model: " << args.at(2).toInt();
        qDebug() << "rows end should be: " << expectedEnd;
        QCOMPARE(args.at(1).toInt(), count); // start
        QCOMPARE(args.at(2).toInt(), expectedEnd); // end
        for (int i = count; i < expectedEnd + 1; i++) {
            Event event1 = model.index(i, 0).data(Qt::UserRole).value<Event>();
            Event event2 = streamModel.index(i, 0).data(Qt::UserRole).value<Event>();
            QVERIFY(compareEvents(event1, event2));
        }

        // You should be able to fetch more if total number of messages is not yet reached:
        if ( args.at(2).toInt() < total-1 )
        {
            QVERIFY(streamModel.canFetchMore(QModelIndex()));
            QVERIFY(modelReady.isEmpty());

            qDebug() << "Calling fetchMore from streaming model...";
            streamModel.fetchMore(QModelIndex());
        }

        count += chunkSize;
    }
    QVERIFY(waitSignal(modelReady, 5000));
    QVERIFY(rowsInserted.isEmpty());
    // TODO: NB#208137
    // QVERIFY(!streamModel.canFetchMore(QModelIndex()));

    modelThread.quit();
    modelThread.wait(3000);
}

void EventModelTest::testModifyInGroup()
{
    EventModel model;
    watcher.setModel(&model);

    // workaround - added DESC(tracker:id) in qsparql branch
    sleep(1);
    Event event;
    event.setType(Event::SMSEvent);
    event.setGroupId(group1.id());
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRemoteUid("td@localhost");
    event.setFreeText("imtest");
    event.setDirection(Event::Outbound);
    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();

    QCOMPARE(watcher.addedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);

    Group group;
    GroupModel groupModel;
    groupModel.enableContactChanges(false);

    QVERIFY(groupModel.trackerIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), event.id());
    QCOMPARE(group.lastMessageText(), event.freeText());
    QCOMPARE(group.lastEventType(), event.type());
    QCOMPARE(group.lastEventStatus(), Event::UnknownStatus);

    // change status
    event.setStatus(Event::SentStatus);
    QVERIFY(model.modifyEventsInGroup(QList<Event>() << event, group));
    watcher.waitForSignals();

    QVERIFY(!model.lastError().isValid());
    QCOMPARE(watcher.updatedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);

    QVERIFY(groupModel.trackerIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), event.id());
    QCOMPARE(group.lastMessageText(), event.freeText());
    QCOMPARE(group.lastEventType(), event.type());
    QCOMPARE(group.lastEventStatus(), Event::SentStatus);

    Event e;
    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));


    // change text
    event.setFreeText("modified text");
    QVERIFY(model.modifyEventsInGroup(QList<Event>() << event, group));
    watcher.waitForSignals();

    QVERIFY(!model.lastError().isValid());
    QCOMPARE(watcher.updatedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);

    QVERIFY(groupModel.trackerIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), event.id());
    QVERIFY(group.lastMessageText() == "modified text");
    QCOMPARE(group.lastEventType(), event.type());
    QCOMPARE(group.lastEventStatus(), Event::SentStatus);

    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));

    // change vcard
    event.setFromVCard("vcard.txt", "Oki Doki");

    QVERIFY(model.modifyEventsInGroup(QList<Event>() << event, group));
    watcher.waitForSignals();

    QVERIFY(!model.lastError().isValid());
    QCOMPARE(watcher.updatedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);

    QVERIFY(groupModel.trackerIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), event.id());
    QVERIFY(group.lastVCardFileName() == "vcard.txt");
    QVERIFY(group.lastVCardLabel() == "Oki Doki");

    QVERIFY(model.trackerIO().getEvent(event.id(), e));
    QVERIFY(compareEvents(event, e));

    // test unread count updating
    int unread = group.unreadMessages();

    sleep(1);
    Event newEvent;
    newEvent.setGroupId(group1.id());
    newEvent.setType(Event::IMEvent);
    newEvent.setDirection(Event::Inbound);
    newEvent.setStartTime(QDateTime::currentDateTime().addSecs(5));
    newEvent.setEndTime(QDateTime::currentDateTime().addSecs(5));
    newEvent.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    newEvent.setRemoteUid("td@localhost");
    newEvent.setFreeText("addEvents 2");

    QVERIFY(model.addEvent(newEvent));
    watcher.waitForSignals();

    QCOMPARE(watcher.addedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);

    QVERIFY(groupModel.trackerIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), newEvent.id());
    QCOMPARE(group.unreadMessages(), unread + 1);

    // mark as read
    newEvent.setIsRead(true);

    QVERIFY(model.modifyEventsInGroup(QList<Event>() << newEvent, group));
    watcher.waitForSignals();

    QVERIFY(!model.lastError().isValid());
    QCOMPARE(watcher.updatedCount(), 1);
    QCOMPARE(watcher.committedCount(), 1);

    QVERIFY(groupModel.trackerIO().getGroup(group1.id(), group));
    QCOMPARE(group.lastEventId(), newEvent.id());
    QCOMPARE(group.unreadMessages(), unread);
}

void EventModelTest::cleanupTestCase()
{
}

QTEST_MAIN(EventModelTest)
