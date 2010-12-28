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

#include <QDBusConnection>
#include "groupmodeltest.h"
#include "groupmodel.h"
#include "event.h"
#include "common.h"
#include "trackerio.h"

using namespace CommHistory;

Group group1, group2;
QEventLoop *loop;
int modifiedGroupId = -1;

QString mms_token1("111-111-111");
QString mms_token2("222-222-222");
QString mms_token3("333-333-333");
QString mms_content_path(".mms/msg");

void GroupModelTest::eventsAddedSlot(const QList<CommHistory::Event> &events)
{
    qDebug() << "eventsAdded:" << events.count();
    loop->exit(0);
}

void GroupModelTest::groupAddedSlot(CommHistory::Group group)
{
    qDebug() << "groupAddedSlot:" << group.id();
    loop->exit(0);
}

void GroupModelTest::groupsUpdatedSlot(const QList<int> &id)
{
    qDebug() << "groupUpdatedSlot:" << id;
    loop->exit(0);
}

void GroupModelTest::groupsUpdatedFullSlot(const QList<CommHistory::Group> &groups)
{
    QVERIFY(!groups.isEmpty());
    Group group = groups.first();
    qDebug() << "groupUpdatedFullSlot:" << group.id();
    modifiedGroupId = group.id();
    loop->exit(0);
}

void GroupModelTest::groupsDeletedSlot(const QList<int> &id)
{
    qDebug() << "groupDeletedSlot:" << id;
    loop->exit(0);
}

void GroupModelTest::dataChangedSlot(const QModelIndex &start, const QModelIndex &end)
{
    Q_UNUSED(start);
    Q_UNUSED(end);
    qDebug() << "dataChanged";
    loop->exit(0);
}

void GroupModelTest::idle(int msec)
{
    QTime timer;
    timer.start();
    while (timer.elapsed() < msec)
        QCoreApplication::processEvents();
}

void GroupModelTest::initTestCase()
{
    deleteAll();

    QVERIFY(QDBusConnection::sessionBus().isConnected());

    EventModel model;

    QVERIFY(QDBusConnection::sessionBus().registerObject(
                "/GroupModelTest", this));
    QVERIFY(QDBusConnection::sessionBus().connect(
                QString(), QString(), "com.nokia.commhistory", "eventsAdded",
                this, SLOT(eventsAddedSlot(const QList<CommHistory::Event> &))));
    QVERIFY(QDBusConnection::sessionBus().connect(
                QString(), QString(), "com.nokia.commhistory", "groupAdded",
                this, SLOT(groupAddedSlot(CommHistory::Group))));
    QVERIFY(QDBusConnection::sessionBus().connect(
                QString(), QString(), "com.nokia.commhistory", "groupsUpdated",
                this, SLOT(groupsUpdatedSlot(const QList<int> &))));
    QVERIFY(QDBusConnection::sessionBus().connect(
                QString(), QString(), "com.nokia.commhistory", "groupsUpdatedFull",
                this, SLOT(groupsUpdatedFullSlot(const QList<CommHistory::Group> &))));
    QVERIFY(QDBusConnection::sessionBus().connect(
                QString(), QString(), "com.nokia.commhistory", "groupsDeleted",
                this, SLOT(groupsDeletedSlot(const QList<int> &))));

    loop = new QEventLoop(this);

    qsrand(QDateTime::currentDateTime().toTime_t());
}

void GroupModelTest::init()
{
    GroupModel model;
    EventModel eventModel;
    model.setQueryMode(EventModel::SyncQuery);

    Group g;
    g.setLocalUid(ACCOUNT1);
    g.setRemoteUids(QStringList() << "td@localhost");
    QVERIFY(model.addGroup(g));
    QVERIFY(g.id() != -1);
    loop->exec();

    QSignalSpy eventsCommitted(&eventModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    // add an event to each group to get them to show up in getGroups()
    addTestEvent(eventModel, Event::IMEvent, Event::Outbound, ACCOUNT1, g.id());
    waitSignal(eventsCommitted, 5000);

    g.setLocalUid(ACCOUNT1);
    g.setRemoteUids(QStringList() << "td2@localhost");
    g.setId(-1);
    QVERIFY(model.addGroup(g));
    loop->exec();
    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT1, g.id());
    waitSignal(eventsCommitted, 5000);


    g.setLocalUid(ACCOUNT2);
    g.setRemoteUids(QStringList() << "td@localhost");
    g.setId(-1);
    QVERIFY(model.addGroup(g));
    loop->exec();
    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, g.id());
    waitSignal(eventsCommitted, 5000);

    g.setLocalUid(ACCOUNT2);
    g.setRemoteUids(QStringList() << "td2@localhost");
    g.setId(-1);
    QVERIFY(model.addGroup(g));
    loop->exec();
    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, g.id());
    waitSignal(eventsCommitted, 5000);

    // TODO: groupsCommitted() would be nice to have - spin loop to make
    // sure all new groups have been added
    idle(2000);
}

void GroupModelTest::cleanup()
{
    deleteAll();
    QDir mms_content(QDir::homePath() + QDir::separator() + mms_content_path);
    mms_content.rmdir(mms_token1);
    mms_content.rmdir(mms_token2);
    mms_content.rmdir(mms_token3);
}

void GroupModelTest::addGroups()
{
    GroupModel model;
    EventModel eventModel;
    model.setQueryMode(EventModel::SyncQuery);

    /* add invalid group */
    QVERIFY(!model.addGroup(group1));
    QVERIFY(model.lastError().isValid());

    group1.setLocalUid(ACCOUNT1);
    group1.setRemoteUids(QStringList() << "td@localhost");
    QVERIFY(model.addGroup(group1));
    QVERIFY(group1.id() != -1);
    loop->exec();
    idle(1000);

    Group group;
    QVERIFY(model.trackerIO().getGroup(group1.id(), group));
    QCOMPARE(group.id(), group1.id());
    QCOMPARE(group.localUid(), group1.localUid());
    QCOMPARE(group.remoteUids(), group1.remoteUids());
    QCOMPARE(group.chatName(), group1.chatName());
    QVERIFY(group.endTime().isValid() == false);
    QCOMPARE(group.totalMessages(), 0);
    QCOMPARE(group.unreadMessages(), 0);
    QCOMPARE(group.sentMessages(), 0);
    QCOMPARE(group.lastEventId(), -1);

    // add an event to each group to get them to show up in getGroups()
    QSignalSpy eventsCommitted(&eventModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    addTestEvent(eventModel, Event::IMEvent, Event::Outbound, ACCOUNT1, group.id());
    waitSignal(eventsCommitted, 5000);

    group2.setLocalUid(ACCOUNT1);
    group2.setRemoteUids(QStringList() << "td2@localhost");
    QVERIFY(model.addGroup(group2));
    loop->exec();
    idle(1000);
    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT1, group2.id());
    waitSignal(eventsCommitted, 5000);

    Group group3;
    group3.setLocalUid(ACCOUNT2);
    group3.setRemoteUids(QStringList() << "td@localhost");
    QVERIFY(model.addGroup(group3));
    loop->exec();
    idle(1000);
    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, group3.id());
    waitSignal(eventsCommitted, 5000);

    Group group4;
    group4.setLocalUid(ACCOUNT2);
    group4.setRemoteUids(QStringList() << "td2@localhost");
    QVERIFY(model.addGroup(group4));
    loop->exec();
    idle(1000);
    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, group4.id());
    waitSignal(eventsCommitted, 5000);
}

void GroupModelTest::modifyGroup()
{
    GroupModel model;

    Group group5;
    group5.setLocalUid(ACCOUNT1);
    group5.setRemoteUids(QStringList() << "td2@localhost");
    group5.setChatName("MUC topic");
    QVERIFY(model.addGroup(group5));
    QVERIFY(group5.id() != -1);
    loop->exec();
    idle(1000);

    Group testGroupA;
    QVERIFY(model.trackerIO().getGroup(group5.id(), testGroupA));
    QCOMPARE(testGroupA.id(), group5.id());
    QCOMPARE(testGroupA.localUid(), group5.localUid());
    QCOMPARE(testGroupA.remoteUids(), group5.remoteUids());
    QCOMPARE(testGroupA.chatName(), group5.chatName());

    Group group6;
    QVERIFY(!model.modifyGroup(group6));
    QVERIFY(model.lastError().isValid());

    group5.setChatName("MUC topic modified");
    QVERIFY(model.modifyGroup(group5));
    loop->exec();
    idle(1000);

    QCOMPARE(modifiedGroupId,group5.id());
    modifiedGroupId = -1;

    Group testGroupB;
    QVERIFY(model.trackerIO().getGroup(group5.id(), testGroupB));
    QCOMPARE(testGroupB.id(), group5.id());
    QCOMPARE(testGroupB.localUid(), group5.localUid());
    QCOMPARE(testGroupB.remoteUids(), group5.remoteUids());
    QCOMPARE(testGroupB.chatName(), group5.chatName());
}

void GroupModelTest::getGroups_data()
{
    QTest::addColumn<bool>("useThread");

    QTest::newRow("Without thread") << false;
    QTest::newRow("Use thread") << true;
}

void GroupModelTest::getGroups()
{
    QFETCH(bool, useThread);

    GroupModel model;
    QSignalSpy modelReady(&model, SIGNAL(modelReady()));
    GroupModel listenerModel;

    QThread modelThread;
    if (useThread) {
        modelThread.start();
        model.setBackgroundThread(&modelThread);
    }

    listenerModel.setQueryMode(EventModel::SyncQuery);

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady, 5000));
    QCOMPARE(model.rowCount(), 4);
    QVERIFY(listenerModel.getGroups());
    QCOMPARE(listenerModel.rowCount(), 4);

    /* add new group */
    Group group;
    group.setLocalUid(ACCOUNT2);
    group.setRemoteUids(QStringList() << "55501234567");
    group.setId(-1);
    QVERIFY(model.addGroup(group));
    loop->exec();
    idle(1000);
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(listenerModel.rowCount(), 5);

    /* filter by localUid */
    modelReady.clear();
    QVERIFY(model.getGroups(ACCOUNT1));
    QVERIFY(waitSignal(modelReady, 5000));
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(listenerModel.getGroups(ACCOUNT1));
    QCOMPARE(listenerModel.rowCount(), 2);

    /* filter out new group */
    group.setLocalUid(ACCOUNT2);
    group.setRemoteUids(QStringList() << "td@localhost");
    group.setId(-1);
    QVERIFY(model.addGroup(group));
    loop->exec();
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    /* add new group */
    group.setLocalUid(ACCOUNT1);
    group.setRemoteUids(QStringList() << "55566601234567");
    group.setId(-1);
    QVERIFY(model.addGroup(group));
    loop->exec();
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(listenerModel.rowCount(), 3);

    idle(1000);

    /* filter by localUid and IM remoteUid */
    modelReady.clear();
    QVERIFY(model.getGroups(ACCOUNT1, "td@localhost"));
    QVERIFY(waitSignal(modelReady, 5000));
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(listenerModel.getGroups(ACCOUNT1, "td@localhost"));
    QCOMPARE(listenerModel.rowCount(), 1);

    /* add new matching group */
    group.setLocalUid(ACCOUNT1);
    group.setRemoteUids(QStringList() << "td@localhost");
    group.setId(-1);
    QVERIFY(model.addGroup(group));
    loop->exec();
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    /* filter out new group */
    group.setRemoteUids(QStringList() << "no@match");
    group.setId(-1);
    QVERIFY(model.addGroup(group));
    loop->exec();
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    idle(1000);

    /* filter by localUid and phone number remoteUid */
    modelReady.clear();
    QVERIFY(model.getGroups(ACCOUNT1, "55566601234567"));
    QVERIFY(waitSignal(modelReady, 5000));
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(listenerModel.getGroups(ACCOUNT1, "55566601234567"));
    QCOMPARE(listenerModel.rowCount(), 1);

    /* add new matching group */
    group.setLocalUid(ACCOUNT1);
    group.setRemoteUids(QStringList() << "+99966601234567");
    group.setId(-1);
    QVERIFY(model.addGroup(group));
    loop->exec();
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    /* filter out new group */
    group.setRemoteUids(QStringList() << "+99966607654321");
    group.setId(-1);
    QVERIFY(model.addGroup(group));
    loop->exec();
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    modelThread.quit();
    modelThread.wait(5000);
}

void GroupModelTest::updateGroups()
{
    GroupModel groupModel;
    groupModel.setQueryMode(EventModel::SyncQuery);
    QVERIFY(groupModel.getGroups(ACCOUNT1));
    connect(&groupModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)),
            this, SLOT(dataChangedSlot(const QModelIndex &, const QModelIndex &)));

    // update last event of group
    idle(1000); // separate event from the rest
    EventModel model;
    QSignalSpy eventsCommitted(&model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));

    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                 groupModel.group(groupModel.index(0, 0)).id(), "added to group");
    QVERIFY(waitSignal(eventsCommitted, 5000));
    eventsCommitted.clear();
    Group group = groupModel.group(groupModel.index(0, 0));
    Event event;
    QVERIFY(group.lastEventId() != -1);
    QVERIFY(model.trackerIO().getEvent(group.lastEventId(), event));
    QCOMPARE(group.lastEventId(), event.id());
    QCOMPARE(group.lastMessageText(), QString("added to group"));
    QCOMPARE(group.endTime().toTime_t(), event.endTime().toTime_t());

    // add new SMS event for second group, check for resorted list, correct contents and date
    sleep(1);
    int id = groupModel.group(groupModel.index(1, 0)).id();
    QDateTime modified = groupModel.index(1, GroupModel::EndTime).data().toDateTime();
    addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT1, id, "sms");
    QVERIFY(waitSignal(eventsCommitted, 5000));
    eventsCommitted.clear();
    group = groupModel.group(groupModel.index(0, 0));
    QCOMPARE(group.id(), id);
    QVERIFY(group.endTime() > modified);
    Group testGroup;
    QVERIFY(groupModel.trackerIO().getGroup(id, testGroup));
    QCOMPARE(testGroup.endTime().toTime_t(), group.endTime().toTime_t());

    // add new IM event for second group, check for resorted list, correct contents and date
    sleep(1);
    id = groupModel.group(groupModel.index(1, 0)).id();
    modified = groupModel.index(1, GroupModel::EndTime).data().toDateTime();
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, id, "sort");
    QVERIFY(waitSignal(eventsCommitted, 5000));
    eventsCommitted.clear();
    group = groupModel.group(groupModel.index(0, 0));
    QCOMPARE(group.id(), id);
    QVERIFY(group.endTime() > modified);

    QVERIFY(groupModel.trackerIO().getGroup(id, testGroup));
    QCOMPARE(testGroup.endTime().toTime_t(), group.endTime().toTime_t());

    // check if status message is really not added to the group
    addTestEvent(model,
                 Event::StatusMessageEvent,
                 Event::Inbound,
                 ACCOUNT1,
                 groupModel.group(groupModel.index(0, 0)).id(),
                 "status message",
                 false,
                 false,
                 QDateTime::currentDateTime(),
                 QString(),
                 true);
    loop->exec();
    group = groupModel.group(groupModel.index(0, 0));
    QVERIFY(group.lastEventId() != -1);
    // we can get the last event
    QVERIFY(model.trackerIO().getEvent(group.lastEventId(), event));
    QVERIFY(group.lastEventId() == event.id());
    // but it is not our status event
    QVERIFY(group.lastMessageText() != QString("status message"));
}

void GroupModelTest::deleteGroups()
{
    GroupModel groupModel;
    GroupModel deleterModel;
    EventModel model;
    Event event;
    int messageId = 0;

    groupModel.setQueryMode(EventModel::SyncQuery);
    QVERIFY(groupModel.getGroups());
    int numGroups = groupModel.rowCount();

    // delete non-existing group
    deleterModel.deleteGroups(QList<int>() << 0);
    loop->exec();
    QCOMPARE(groupModel.rowCount(), numGroups);

    // delete first group
    messageId = groupModel.group(groupModel.index(0, 0)).lastEventId();
    deleterModel.deleteGroups(QList<int>() << groupModel.group(groupModel.index(0, 0)).id());
    loop->exec();
    idle(1000);
    QCOMPARE(groupModel.rowCount(), numGroups - 1);
    QVERIFY(!model.trackerIO().getEvent(messageId, event));

    // delete group without deleting messages
    messageId = groupModel.group(groupModel.index(0, 0)).lastEventId();
    deleterModel.deleteGroups(QList<int>() << groupModel.group(groupModel.index(0, 0)).id(),
                              false);
    loop->exec();
    idle(1000);
    QCOMPARE(groupModel.rowCount(), numGroups - 2);
    QVERIFY(model.trackerIO().getEvent(messageId, event));

    // delete group with SMS/MMS, check that messages are flagged as deleted
    QSignalSpy groupDataChanged(&groupModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)));
    int groupId = groupModel.group(groupModel.index(0, 0)).id();
    Event sms;
    sms.setType(Event::SMSEvent);
    sms.setDirection(Event::Outbound);
    sms.setGroupId(groupId);
    sms.setStartTime(QDateTime::currentDateTime());
    sms.setEndTime(QDateTime::currentDateTime());
    sms.setLocalUid(ACCOUNT1);
    sms.setRemoteUid("01234567");
    sms.setFreeText("smstest");
    QVERIFY(model.addEvent(sms));
    QVERIFY(waitSignal(groupDataChanged, 5000));
    groupDataChanged.clear();

    Event mms;
    mms.setType(Event::MMSEvent);
    mms.setDirection(Event::Outbound);
    mms.setGroupId(groupId);
    mms.setStartTime(QDateTime::currentDateTime());
    mms.setEndTime(QDateTime::currentDateTime());
    mms.setLocalUid(ACCOUNT1);
    mms.setRemoteUid("01234567");
    mms.setFreeText("mmstest");
    QVERIFY(model.addEvent(mms));
    QVERIFY(waitSignal(groupDataChanged, 5000));
    groupDataChanged.clear();

    deleterModel.deleteGroups(QList<int>() << groupModel.group(groupModel.index(0, 0)).id());
    loop->exec();
    idle(1000);
    QCOMPARE(groupModel.rowCount(), numGroups - 3);

    QVERIFY(!model.trackerIO().getEvent(sms.id(), event));
    QVERIFY(!model.trackerIO().getEvent(mms.id(), event));
}

void GroupModelTest::streamingQuery_data()
{
    QTest::addColumn<bool>("useThread");

    QTest::newRow("Use thread") << true;
    QTest::newRow("Without thread") << false;
}

void GroupModelTest::streamingQuery()
{
    QFETCH(bool, useThread);

    GroupModel groupModel;
    groupModel.setQueryMode(EventModel::SyncQuery);
    GroupModel streamModel;

    QThread modelThread;
    if (useThread) {
        modelThread.start();
        streamModel.setBackgroundThread(&modelThread);
    }

    EventModel eventModel;
    QSignalSpy eventsCommitted(&eventModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    // insert some query folder
    for (int i = 0; i < 10; i++) {
        group1.setId(-1);
        QVERIFY(groupModel.addGroup(group1));
        loop->exec();
        addTestEvent(eventModel, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id());
        QVERIFY(waitSignal(eventsCommitted, 5000));
    }
    idle(2000);
    QVERIFY(groupModel.getGroups());

    int total = groupModel.rowCount();
    qDebug() << "total groups: " << total;
    QVERIFY(total >= 10);

    const int normalChunkSize = 5;
    const int firstChunkSize = 3;
    streamModel.setQueryMode(EventModel::StreamedAsyncQuery);
    streamModel.setChunkSize(normalChunkSize);
    streamModel.setFirstChunkSize(firstChunkSize);
    qRegisterMetaType<QModelIndex>("QModelIndex");
    QSignalSpy rowsInserted(&streamModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)));
    QSignalSpy modelReady(&streamModel, SIGNAL(modelReady()));
    QVERIFY(streamModel.getGroups());

    QList<int> idsOrig;
    QList<int> idsStream;
    QTime timer;
    int count = 0;
    int chunkSize = firstChunkSize;
    while (count < total) {
        qDebug() << "count: " << count;
        if (count > 0)
            chunkSize = normalChunkSize;

        qDebug() << "chunk size: " << chunkSize;

        QVERIFY(waitSignal(rowsInserted, 5000));

        QList<QVariant> args = rowsInserted.takeFirst();
        bool lastBatch = count + chunkSize >= total;
        int expectedEnd = lastBatch ? total -1 : count + chunkSize -1;
        qDebug() << "rows start in streaming model: " << args.at(1).toInt();
        qDebug() << "rows start should be: " << count;
        qDebug() << "rows end in streaming model: " << args.at(2).toInt();
        qDebug() << "rows end should be: " << expectedEnd;
        QCOMPARE(args.at(1).toInt(), count); // start
        QCOMPARE(args.at(2).toInt(), expectedEnd); // end
        for (int i = count; i < expectedEnd + 1; i++) {
            Group group1 = groupModel.group(groupModel.index(i, 0));
            Group group2 = streamModel.group(streamModel.index(i, 0));
            QCOMPARE(group1.endTime(),group2.endTime());
            idsOrig.append(group1.id());
            idsStream.append(group2.id());
            //QCOMPARE(group1.id(), group2.id()); // Cannot compare like this, because groups having same timestamp
            //can be in random order in data model.
        }

        // You should be able to fetch more if total number of groups is not yet reached:
        if ( args.at(2).toInt() < total-1 )
        {
            QVERIFY(streamModel.canFetchMore(QModelIndex()));
        }

        if (!lastBatch)
            QVERIFY(modelReady.isEmpty());

        qDebug() << "Calling fetchMore from streaming model...";
        streamModel.fetchMore(QModelIndex());
        count += chunkSize;
    }
    QVERIFY(waitSignal(modelReady, 5000));
    QVERIFY(rowsInserted.isEmpty());
    // TODO: NB#208137
    //QVERIFY(!streamModel.canFetchMore(QModelIndex()));
    QCOMPARE(idsOrig.toSet().size(), idsOrig.size());
    QCOMPARE(idsStream.toSet().size(), idsStream.size());
    QVERIFY(idsOrig.toSet() == idsStream.toSet());

    modelThread.quit();
    modelThread.wait(5000);
}

void GroupModelTest::cleanupTestCase()
{
//    deleteAll();
}

int addTestMms (EventModel &model,
                 Event::EventDirection direction,
                 const QString &account,
                 int groupId,
                 const QString messageToken = QString(),
                 const QString &text = QString("test event"))
{
    int id = addTestEvent(model,
                 Event::MMSEvent,
                 direction,
                 account,
                 groupId,
                 text,
                 false,
                 false,
                 QDateTime::currentDateTime(),
                 QString(),
                 false,
                 messageToken);
     qDebug() << "Added test MMS message:" << id;

     return id;
}

void GroupModelTest::deleteMmsContent()
{
    Group group1, group2;
    GroupModel model;
    EventModel eventModel;
    Event e;
    int id1, id2, id3;

    QDir content_dir(QDir::homePath() + QDir::separator() + mms_content_path);
    content_dir.mkdir(mms_token1);
    content_dir.mkdir(mms_token2);
    content_dir.mkdir(mms_token3);

    model.setQueryMode(EventModel::SyncQuery);

    // Create 3 groups
    group1.setLocalUid(ACCOUNT1);
    group1.setRemoteUids(QStringList() << "mms-test1@localhost");
    QVERIFY(model.addGroup(group1));
    QVERIFY(group1.id() != -1);
    loop->exec();
    idle(1000);

    group2.setLocalUid(ACCOUNT1);
    group2.setRemoteUids(QStringList() << "mms-test2@localhost");
    QVERIFY(model.addGroup(group2));
    loop->exec();
    idle(1000);

    Group group3;
    group3.setLocalUid(ACCOUNT1);
    group3.setRemoteUids(QStringList() << "mms-test3@localhost");
    QVERIFY(model.addGroup(group3));
    loop->exec();
    idle(1000);

    // Populate groups
    // ev1 -> token1

    // ev2 -> token2
    // ev3 -> token2

    // ev4 -> token3
    // ev5 -> token3
    // ev6 -> token3

    QSignalSpy eventsCommitted(&eventModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    id1 = addTestMms(eventModel, Event::Inbound, ACCOUNT1, group1.id(), mms_token1, "test MMS to delete");
    waitSignal(eventsCommitted, 5000);

    id2 = addTestMms(eventModel, Event::Outbound, ACCOUNT1, group2.id(), mms_token2, "test MMS to delete");
    waitSignal(eventsCommitted, 5000);

    id3 = addTestMms(eventModel, Event::Outbound, ACCOUNT1, group3.id(), mms_token2, "test MMS to delete");
    waitSignal(eventsCommitted, 5000);

    addTestMms(eventModel, Event::Inbound, ACCOUNT1, group1.id(), mms_token3, "test MMS to delete");
    waitSignal(eventsCommitted, 5000);

    addTestMms(eventModel, Event::Inbound, ACCOUNT1, group2.id(), mms_token3, "test MMS to delete");
    waitSignal(eventsCommitted, 5000);

    addTestMms(eventModel, Event::Inbound, ACCOUNT1, group3.id(), mms_token3, "test MMS to delete");
    waitSignal(eventsCommitted, 5000);


    QVERIFY(model.trackerIO().getEvent(id1, e));
    QVERIFY(model.trackerIO().deleteEvent(e, 0));
    QTest::qWait(1000);
    QVERIFY(content_dir.exists(mms_token1) == false); // folder shall be removed since no more events reffers to the token

    QVERIFY(model.trackerIO().getEvent(id2, e));
    QVERIFY(model.trackerIO().deleteEvent(e, 0));
    QTest::qWait(1000);
    QVERIFY(content_dir.exists(mms_token2) == true);  // one more events refers to token2

    QVERIFY(model.trackerIO().getEvent(id3, e));
    QVERIFY(model.trackerIO().deleteEvent(e, 0));
    QTest::qWait(1000);
    QVERIFY(content_dir.exists(mms_token2) == false);  // no more events with token2

    model.deleteGroups(QList<int>() << group1.id());
    QTest::qWait(1000);
    QVERIFY(content_dir.exists(mms_token3) == true);

    model.deleteGroups(QList<int>() << group1.id() << group2.id() << group3.id());
    QTest::qWait(1000);
    QVERIFY(content_dir.exists(mms_token3) == false);
}

QTEST_MAIN(GroupModelTest)
