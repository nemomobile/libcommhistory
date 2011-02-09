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
#include "syncmodeltest.h"
#include "conversationmodel.h"
#include "event.h"
#include "common.h"
#include "modelwatcher.h"

using namespace CommHistory;

Group group;
QEventLoop loop;
int eventCounter;
ModelWatcher watcher;

void SyncModelTest::initTestCase()
{
    QVERIFY(QDBusConnection::sessionBus().isConnected());
    deleteAll();
    watcher.setLoop(&loop);
    numAddedEvents = 0;
    qsrand(QDateTime::currentDateTime().toTime_t());

    addTestGroup(group, "121", "122");
}

void SyncModelTest::init()
{
    itemId = -1;
    deleteSmsMsgs();
    numAddedEvents = 0;
}

void SyncModelTest::addSmsEvents_data()
{
    QTest::addColumn<int>("parentId");
    QTest::addColumn<QDateTime>("getTime");
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("read");

    QDateTime timeStart = QDateTime::currentDateTime().toUTC();
    timeStart = timeStart.addSecs(-10); // a small offset just to make sure that timeStart
                                                                     //refers to a time which is before the time at which msgs will get added
    //Inbox
    QTest::newRow("Inbox1") << 4098 << timeStart << "hello inbox 1" << false;

    //Outbox
    QTest::newRow("Outbox1") << 4099 << timeStart << "hello outbox 1" << false;

    //Sent
    QTest::newRow("Sent1") << 4101 << timeStart << "hello sent 1" << false;

    //MyFolder
    QTest::newRow("MyFolder") << 4104 << timeStart << "hello myfolder " << false;
}


//This test case checks whether all events get added properly
void SyncModelTest::addSmsEvents()
{
    QFETCH(int, parentId);
    QFETCH(QDateTime, getTime);
    QFETCH(QString, text);
    QFETCH(bool, read);

    QDateTime sentReceivedTime = QDateTime::fromTime_t(qrand());
    //Adding event to db
    QVERIFY(addEvent(parentId, group.id(), sentReceivedTime, "121", "122", text, read));

    //Retrieving the stored event based on parentId and time filter
    SyncSMSModel model;
    model.setQueryMode(EventModel::SyncQuery);
    SyncSMSFilter filter;
    filter.parentId = parentId;
    filter.time = getTime;
    filter.lastModified = false;
    model.setSyncSmsFilter(filter);
    QVERIFY(model.getEvents());
    int numEventsRetrieved = model.rowCount();

    QVERIFY(numEventsRetrieved == 1); //we are adding events of different parents ids, each 1
    QModelIndex idx = model.index(0, 0);
    QVERIFY(idx.isValid());

    Event e = model.event(idx);
    QVERIFY(e.isValid());
    QVERIFY(e.parentId() == parentId);
    QVERIFY(e.startTime().toTime_t() == sentReceivedTime.toTime_t());
    QVERIFY(e.endTime().toTime_t() == sentReceivedTime.toTime_t());
    QVERIFY(e.freeText() == text);
    QVERIFY(e.isRead() == read);
    QVERIFY(e.localUid() == "121");
    QVERIFY(e.remoteUid() == "122");

}

void SyncModelTest::readAddedSmsEventsFromConvModel()
{
    //Adding 2 sms events to  group
    qsrand(QDateTime::currentDateTime().toTime_t());
    QDateTime sentReceivedTime = QDateTime::fromTime_t(qrand());
    QVERIFY(addEvent(4098, group.id(), sentReceivedTime, "121", "122", "Added grouped msg 1 # 121-122", false));
    QVERIFY(addEvent(4098, group.id(), sentReceivedTime, "121", "122", "Added grouped msg 2 # 121-122", false));

    //getting the first event and comparing
    ConversationModel convModel;
    convModel.enableContactChanges(false);
    convModel.setQueryMode(EventModel::SyncQuery);
    QVERIFY(convModel.getEvents(group.id()));
    QVERIFY(convModel.rowCount() == 2);

    for (int i = 0; i < 2; i++) {
        Event e = convModel.event(convModel.index(i, 0));
        if (i == 0) {
            QVERIFY(e.freeText() == "Added grouped msg 2 # 121-122");
        } else if (i == 1) {
            QVERIFY(e.freeText() == "Added grouped msg 1 # 121-122");
        }
    }

}

// getModifiedItems(t) ==> needs to return all sms whose modified time is greater than t, but were created before t(tracker::added <= t)
void SyncModelTest::addModifyGetSingleSmsEvents()
{
    QDateTime time_1 = QDateTime::currentDateTime().toUTC();//approx time when msg A gets added to tracker
    qsrand(QDateTime::currentDateTime().toTime_t());
    QDateTime sentReceivedTime = QDateTime::fromTime_t(qrand());
    QVERIFY(addEvent(4098, group.id(), sentReceivedTime, "121", "122", "Msg A", false));
    int id_1 = itemId;

    QDateTime time_2 = time_1.addSecs(60*5);
    QDateTime time_3 = time_2.addSecs(60*5);
    QVERIFY(modifyEvent(id_1, 4098, group.id(), time_3, "121", "122", "Msg A", false)); //modify A at time B

    SyncSMSModel model;
    SyncSMSFilter filter(4098, time_2, true);
    model.setSyncSmsFilter(filter);
    model.setQueryMode(EventModel::SyncQuery);
    QVERIFY(model.getEvents());
    QVERIFY(model.rowCount() == 1);
    evaluateModel(model, QStringList() << "Msg A");
}

void SyncModelTest::cleanupTestCase()
{
}

//Private functions
bool  SyncModelTest::addEvent( int parentId, int groupId, const QDateTime& sentReceivedTime,
                               const QString& localId, const QString& remoteId, const QString& text, bool read)
{
    EventModel model;
    watcher.setModel(&model);
    Event e;
    e.setType(Event::SMSEvent);
    e.setParentId(parentId);

    if (parentId == ::INBOX ||  parentId >= ::MYFOLDER) {
        e.setDirection(Event::Inbound);
    }  else {
        e.setDirection(Event::Outbound);
    }
    e.setGroupId(groupId);
    e.setStartTime(sentReceivedTime);
    e.setEndTime(sentReceivedTime);
    e.setLocalUid(localId);
    e.setRemoteUid(remoteId);
    e.setFreeText(text);
    e.setIsRead(read);

    bool ret_val =  model.addEvent(e);
    watcher.waitForSignals(1, 1);
    itemId = e.id();
    return ret_val;
}

bool  SyncModelTest::modifyEvent( int itemId, int parentId, int groupId, const QDateTime &lastModTime,
                                  const QString& localId, const QString& remoteId, const QString& text, bool read)
{
    EventModel model;
    watcher.setModel(&model);
    Event e;
    e.setType(Event::SMSEvent);
    e.setId(itemId);
    e.setParentId(parentId);

    if (parentId == ::INBOX ||  parentId >= ::MYFOLDER) {
        e.setDirection(Event::Inbound);
    }  else {
        e.setDirection(Event::Outbound);
    }
    e.setGroupId(groupId);
    e.setLastModified(lastModTime);
    e.setLocalUid(localId);
    e.setRemoteUid(remoteId);
    e.setFreeText(text);
    e.setIsRead(read);

    bool    ret = model.modifyEvent(e);
    watcher.waitForSignals(1, 1);
    return ret;
}

//model.getEvents() is called b4 this
void SyncModelTest::evaluateModel(const SyncSMSModel &model, const QStringList &expectedListMsgs)
{
    int rowCount = model.rowCount();
    QCOMPARE(rowCount, expectedListMsgs.count());

    for (int i = 0; i < rowCount; i++) {
        Event e = model.event(model.index(i, 0));
        QCOMPARE(e.freeText(), expectedListMsgs.at(i));
    }
}

QTEST_MAIN(SyncModelTest)
