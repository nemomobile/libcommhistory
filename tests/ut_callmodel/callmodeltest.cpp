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
#include "callmodeltest.h"
#include "common.h"
#include "modelwatcher.h"

using namespace CommHistory;

Group group1, group2;
Event im, sms, call;
QEventLoop loop;
int eventCounter;

ModelWatcher watcher;

const QString REMOTEUID1( "user1@remotehost" );
const QString REMOTEUID2( "user2@remotehost" );

class TestCallItem
{
public:
    TestCallItem( const QString &remoteUid, CallEvent::CallType callType, int eventCount )
            : remoteUid( remoteUid )
            , callType( callType )
            , eventCount( eventCount )
        {};

    QString remoteUid;
    CallEvent::CallType callType;
    int eventCount;
};

QList<TestCallItem> testCalls;


void CallModelTest::initTestCase()
{
    QVERIFY( QDBusConnection::sessionBus().isConnected() );

    deleteAll();

    watcher.setLoop(&loop);

    qsrand( QDateTime::currentDateTime().toTime_t() );

    addTestGroups( group1, group2 );

    // add 8 call events from user1
    int cnt = 0;
    QDateTime when = QDateTime::currentDateTime();

    EventModel model;
    watcher.setModel(&model);
    // 2 dialed
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when,               REMOTEUID1 ); cnt++;
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(  5 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::DialedCallType, 2 ) );

    // 1 missed
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, true,  when.addSecs( 10 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::MissedCallType, 1 ) );

    // 2 received
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, false, when.addSecs( 15 ), REMOTEUID1 ); cnt++;
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, false, when.addSecs( 20 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::ReceivedCallType, 2 ) );

    // 1 dialed
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs( 25 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::DialedCallType, 1 ) );

    // 2 missed
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, true,  when.addSecs( 30 ), REMOTEUID1 ); cnt++;
    addTestEvent( model, Event::CallEvent, Event::Inbound,  ACCOUNT1, -1, "", false, true,  when.addSecs( 35 ), REMOTEUID1 ); cnt++;
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::MissedCallType, 2 ) );

    // add 1 im and 2 sms events
    addTestEvent( model, Event::IMEvent,   Event::Outbound, ACCOUNT1, group1.id(), "test" );        cnt++;
    addTestEvent( model, Event::SMSEvent,  Event::Inbound,  ACCOUNT1, group1.id(), "test" );        cnt++;
    addTestEvent( model, Event::SMSEvent,  Event::Outbound, ACCOUNT1, group2.id(), "draft", true ); cnt++;

    watcher.waitForSignals(cnt);
}

void CallModelTest::testGetEvents( CallModel::Sorting sorting, int row_count, QList<TestCallItem> calls )
{
    CallModel model;
    model.enableContactChanges(false);
    model.setQueryMode(EventModel::SyncQuery);

    qDebug() << __PRETTY_FUNCTION__ << "*** Sorting by " << (int)sorting;
    model.setFilter(  sorting  );
    QVERIFY( model.getEvents() );

    qDebug() << "Top level event(s):" << row_count;
    QCOMPARE( model.rowCount(), row_count );

    for ( int i = 0; i < row_count; i++ )
    {
        Event e = model.event( model.index( i, 0 ) );
        qDebug() << "EVENT:" << e.id() << "|" << e.remoteUid() << "|" << e.direction() << "|" << e.isMissedCall() << "|" << e.eventCount();
        QCOMPARE( e.type(), Event::CallEvent );
        QCOMPARE( e.remoteUid(), calls.at( i ).remoteUid );
        switch ( calls.at( i ).callType )
        {
            case CallEvent::MissedCallType :
            {
                QCOMPARE( e.direction(), Event::Inbound );
                QCOMPARE( e.isMissedCall(), true );
                QCOMPARE( e.eventCount(), calls.at( i ).eventCount );
                break;
            }
            case CallEvent::ReceivedCallType :
            {
                QCOMPARE( e.direction(), Event::Inbound );
                QCOMPARE( e.isMissedCall(), false );
                // received and missed calls have invalid event count if sorted by contact
                QCOMPARE( e.eventCount(), sorting == CallModel::SortByContact ? -1 : calls.at( i ).eventCount );
                break;
            }
            case CallEvent::DialedCallType :
            {
                QCOMPARE( e.direction(), Event::Outbound );
                QCOMPARE( e.isMissedCall(), false );
                // received and missed calls have invalid event count if sorted by contact
                QCOMPARE( e.eventCount(), sorting == CallModel::SortByContact ? -1 : calls.at( i ).eventCount );
                break;
            }
            default :
            {
                qCritical() << "Unknown call type!";
                return;
            }
        }
    }
}

void CallModelTest::testAddEvent()
{
    CallModel model;
    model.enableContactChanges(false);
    watcher.setModel(&model);
    model.setQueryMode( EventModel::SyncQuery );

    /* by contact:
     * -----------
     * user1, missed   (2)
     */
    testGetEvents( CallModel::SortByContact, 1, testCalls );
    /* by time:
     * --------
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 1 dialed from user1
    QDateTime when = QDateTime::currentDateTime();
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs( 40 ), REMOTEUID1 );
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::DialedCallType, 1 ) );
    watcher.waitForSignals();

    /* by contact:
     * -----------
     * user1, dialed   (-1)
     */
    testGetEvents( CallModel::SortByContact, 1, testCalls );
    /* by time:
     * --------
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 5 received from user1
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 45 ), REMOTEUID1 );
    watcher.waitForSignals();
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 50 ), REMOTEUID1 );
    watcher.waitForSignals();
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 55 ), REMOTEUID1 );
    watcher.waitForSignals();
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 60 ), REMOTEUID1 );
    watcher.waitForSignals();
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 65 ), REMOTEUID1 );
    watcher.waitForSignals();
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::ReceivedCallType, 5 ) );

    /* by contact:
     * -----------
     * user1, received (-1)
     */
    testGetEvents( CallModel::SortByContact, 1, testCalls );
    /* by time:
     * --------
     * user1, received (5)
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 1 missed from user2
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs( 70 ), REMOTEUID2 );
    testCalls.insert( 0, TestCallItem( REMOTEUID2, CallEvent::MissedCallType, 1 ) );
    watcher.waitForSignals();

    /* by contact:
     * -----------
     * user2, missed   (1)
     * user1, received (-1)
     */
    testGetEvents( CallModel::SortByContact, 2, testCalls );
    /* by time:
     * --------
     * user2, received (1)
     * user1, received (5)
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 1 received from user1
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 75 ), REMOTEUID1 );
    testCalls.insert( 0, TestCallItem( REMOTEUID1, CallEvent::ReceivedCallType, 1 ) );
    watcher.waitForSignals();

    /* by contact: ***REORDERING
     * -----------
     * user1, received (-1)
     * user2, missed   (1)
     */
    testGetEvents( CallModel::SortByContact, 2, testCalls );
    /* by time:
     * --------
     * user1, received (1)
     * user2, missed   (1)
     * user1, received (5)
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    // add 1 received from user2
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs( 80 ), REMOTEUID2 );
    testCalls.insert( 0, TestCallItem( REMOTEUID2, CallEvent::ReceivedCallType, 1 ) );
    watcher.waitForSignals();

    /* by contact:
     * -----------
     * user2, received (-1)
     * user1, received (-1)
     */
    testGetEvents( CallModel::SortByContact, 2, testCalls );
    /* by time:
     * --------
     * user2, received (1)
     * user1, received (1)
     * user2, missed   (1)
     * user1, received (5)
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );
}

void CallModelTest::testDeleteEvent()
{
    CallModel model;
    model.enableContactChanges(false);
    watcher.setModel(&model);

    // force change of sorting to SortByContact
    QVERIFY( model.setFilter( CallModel::SortByContact ) );
    QVERIFY( model.getEvents() );
    QVERIFY(watcher.waitForModelReady(5000));

    /* by contact:
     * -----------
     * user2, received (-1)***
     * user1, received (-1)
     */
    // take the event
    Event e = model.event( model.index( 0, 0 ) );
    QVERIFY( e.isValid() );
    QCOMPARE( e.type(), Event::CallEvent );
    qDebug() << "EVENT:" << e.id() << "|" << e.remoteUid() << "|" << e.direction() << "|" << e.isMissedCall() << "|" << e.eventCount();
    QCOMPARE( e.direction(), Event::Inbound );
    QCOMPARE( e.isMissedCall(), false );
    // delete it
    QVERIFY( model.deleteEvent( e.id() ) );
    watcher.waitForSignals();
    QCOMPARE(watcher.deletedCount(), 1);
    // correct test helper lists to match current situation
    testCalls.takeFirst(); testCalls.takeFirst(); testCalls.takeFirst();
    // test if model contains what we want it does
    testGetEvents( CallModel::SortByContact, 1, testCalls );


    // force change of sorting to SortByTime
    QVERIFY( model.setFilter( CallModel::SortByTime ) );
    QVERIFY(watcher.waitForModelReady(5000));

    /* by time:
     * --------
     * user1, received (6)***
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     *
     *     ||
     *     \/
     *
     * user1, dialed   (1)
     * user1, missed   (2)
     * user1, dialed   (1)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    // take the event
    e = model.event( model.index( 0, 0 ) );
    QVERIFY( e.isValid() );
    QCOMPARE( e.type(), Event::CallEvent );
    qDebug() << "EVENT:" << e.id() << "|" << e.remoteUid() << "|" << e.direction() << "|" << e.isMissedCall() << "|" << e.eventCount();
    QCOMPARE( e.direction(), Event::Inbound );
    QCOMPARE( e.isMissedCall(), false );
    // delete it
    QVERIFY( model.deleteEvent( e.id() ) );
    watcher.waitForSignals();
    // correct test helper lists to match current situation
    testCalls.takeFirst();
    // test if model contains what we want it does
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );

    /* by time:
     * --------
     * user1, dialed   (1) <---\
     * user1, missed   (2)***   regrouping
     * user1, dialed   (1) <---/
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     *
     *     ||
     *     \/
     *
     * user1, dialed   (2)
     * user1, received (2)
     * user1, missed   (1)
     * user1, dialed   (2)
     */
    // take the event
    e = model.event( model.index( 1, 0 ) );
    QVERIFY( e.isValid() );
    QCOMPARE( e.type(), Event::CallEvent );
    qDebug() << "EVENT:" << e.id() << "|" << e.remoteUid() << "|" << e.direction() << "|" << e.isMissedCall() << "|" << e.eventCount();
    QCOMPARE( e.direction(), Event::Inbound );
    QCOMPARE( e.isMissedCall(), true );
    // delete it
    QVERIFY( model.deleteEvent( e.id() ) );
    watcher.waitForSignals();
    // correct test helper lists to match current situation
    testCalls.takeFirst(); testCalls.takeFirst(); testCalls.first().eventCount = 2;
    // test if model contains what we want it does
    testGetEvents( CallModel::SortByTime, testCalls.count(), testCalls );


    // force change of sorting to SortByContact
    QVERIFY( model.setFilter( CallModel::SortByContact ) );
    QVERIFY(watcher.waitForModelReady(5000));
    /* by contact:
     * -----------
     * user1, dialed (-1)***
     *
     *    ||
     *    \/
     *
     * (empty)
     */
    // take the event
    e = model.event( model.index( 0, 0 ) );
    QVERIFY( e.isValid() );
    QCOMPARE( e.type(), Event::CallEvent );
    qDebug() << "EVENT:" << e.id() << "|" << e.remoteUid() << "|" << e.direction() << "|" << e.isMissedCall() << "|" << e.eventCount();
    QCOMPARE( e.direction(), Event::Outbound );
    QCOMPARE( e.isMissedCall(), false );
    // delete it
    QVERIFY( model.deleteEvent( e.id() ) );
    watcher.waitForSignals();
    // correct test helper lists to match current situation
    testCalls.clear();
    // test if model contains what we want it does
    testGetEvents( CallModel::SortByContact, 0, testCalls );
}

void CallModelTest::testGetEventsTimeTypeFilter_data()
{
    QTest::addColumn<bool>("useThread");

    QTest::newRow("Without thread") << false;
    QTest::newRow("Use thread") << true;
}

void CallModelTest::testGetEventsTimeTypeFilter()
{
    QFETCH(bool, useThread);

    deleteAll();

    QThread modelThread;

    //initTestCase ==> 3 dialled calls, 2 Received calls, 3 Missed Calls already added
    CallModel model;
    model.enableContactChanges(false);
    watcher.setModel(&model);
    if (useThread) {
        modelThread.start();
        model.setBackgroundThread(&modelThread);
    }

    QDateTime when = QDateTime::currentDateTime();
    //3 dialled
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when );
    watcher.waitForSignals(1);
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(5) );
    watcher.waitForSignals(1);
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs(10) );
    watcher.waitForSignals(1);

    //2 received
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when );
    watcher.waitForSignals(1);
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, when.addSecs(5) );
    watcher.waitForSignals(1);

    //3 missed
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when );
    watcher.waitForSignals(1);
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(5) );
    watcher.waitForSignals(1);
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when.addSecs(10) );
    watcher.waitForSignals(1);

    QDateTime time = when;
    //model.setQueryMode(EventModel::SyncQuery);
    model.setTreeMode(false);
    QVERIFY(model.setFilter(CallModel::SortByTime,  CallEvent::DialedCallType, time));
    QVERIFY(model.getEvents());
    QVERIFY(watcher.waitForModelReady(5000));

    int numEventsRet = model.rowCount();
    QCOMPARE(numEventsRet, 3);
    Event e1 = model.event(model.index(0,0));
    Event e2 = model.event(model.index(1,0));
    Event e3 = model.event(model.index(2,0));
    QVERIFY(e1.isValid());
    QVERIFY(e2.isValid());
    QVERIFY(e3.isValid());
    QVERIFY(e1.direction() == Event::Outbound);
    QVERIFY(e2.direction() == Event::Outbound);
    QVERIFY(e3.direction() == Event::Outbound);

    QVERIFY(model.setFilter(CallModel::SortByTime, CallEvent::MissedCallType, time));
    QVERIFY(watcher.waitForModelReady(5000));
    QVERIFY(model.rowCount() == 3);
    QVERIFY(model.setFilter(CallModel::SortByTime, CallEvent::ReceivedCallType, time));
    QVERIFY(watcher.waitForModelReady(5000));
    QVERIFY(model.rowCount() == 2);

    /**
      * testing to check for adding events with wrong filters
      */
    time = when.addSecs(-60*5);
    int numEventsGot = 0;
    //adding one more received but 5 minutes before the set time filter
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, false, time );
    watcher.waitForSignals(1);
    QVERIFY(model.rowCount() == 2); //event should not be added to model, so rowCount should remain same for received calls
    //filter is set for received call, try to add missed and dialled calls with correct time filter
    addTestEvent( model, Event::CallEvent, Event::Inbound, ACCOUNT1, -1, "", false, true, when );
    watcher.waitForSignals(1);
    numEventsGot = model.rowCount();
    QVERIFY(numEventsGot == 2); //event should not be added to model, so rowCount should remain same which was for received calls
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when );
    watcher.waitForSignals(1);
    numEventsGot = model.rowCount();
    QVERIFY(numEventsGot  == 2); //event should not be added to model, so rowCount should remain same which was for received calls

    /**
      ** testing to check for getting events after he time when all events addition was complete
      */
    //Trying to get events after 5 minutes after the  first event was added
    time = when.addSecs(60*5);
    QVERIFY(model.setFilter(CallModel::SortByTime, CallEvent::ReceivedCallType, time));
    QVERIFY(watcher.waitForModelReady(5000));
    QVERIFY(model.rowCount() == 0);
}

void CallModelTest::deleteAllCalls()
{
    CallModel model;
    watcher.setModel(&model);
    model.setQueryMode(EventModel::SyncQuery);
    QDateTime when = QDateTime::currentDateTime();
    addTestEvent( model, Event::CallEvent, Event::Outbound, ACCOUNT1, -1, "", false, false, when.addSecs( 54 ), REMOTEUID1 );
    watcher.waitForSignals();

    QVERIFY(model.getEvents());
    int callCount = model.rowCount();
    QVERIFY(callCount > 0);
    qDebug() << __PRETTY_FUNCTION__ << "Found out " << callCount << " calls.";
    QSignalSpy eventsCommitted(&model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(model.deleteAll());
    waitSignal(eventsCommitted, 5000);

    QVERIFY(model.getEvents());
    callCount = model.rowCount();
    qDebug() << __PRETTY_FUNCTION__ << "Found out " << callCount << " calls after deleteAll()";
    QCOMPARE(callCount,0);
}

void CallModelTest::cleanupTestCase()
{
}

QTEST_MAIN(CallModelTest)
