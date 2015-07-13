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
#include "groupmodeltest.h"
#include "groupmodel.h"
#include "event.h"
#include "common.h"
#include "databaseio.h"

using namespace CommHistory;

Group group1, group2;
QEventLoop *loop;
int modifiedGroupId = -1;
int addedGroupId = -1;

#define mms_token1 QLatin1String("111-111-111")
#define mms_token2 QLatin1String("222-222-222")
#define mms_token3 QLatin1String("333-333-333")
#define mms_content_path QLatin1String(".mms/msg")

#define ADD_GROUPS_NUM 30

void GroupModelTest::eventsAddedSlot(const QList<CommHistory::Event> &events)
{
    qDebug() << "eventsAdded:" << events.count();
    loop->exit(0);
}

void GroupModelTest::groupsAddedSlot(const QList<CommHistory::Group> &groups)
{
    qDebug() << "groupsAddedSlot:" << groups.count();
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
                QString(), QString(), "com.nokia.commhistory", "groupsAdded",
                this, SLOT(groupsAddedSlot(const QList<CommHistory::Group> &))));
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
    EventModel eventModel;

    Group g;
    addTestGroup(g,ACCOUNT1,QString("td@localhost"));
    QVERIFY(g.id() != -1);

    QSignalSpy eventsCommitted(&eventModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    // add an event to each group to get them to show up in getGroups()
    addTestEvent(eventModel, Event::IMEvent, Event::Outbound, ACCOUNT1, g.id());
    QVERIFY(waitSignal(eventsCommitted));

    g.setId(-1);
    addTestGroup(g,ACCOUNT1,QString("td2@localhost"));

    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT1, g.id());
    QVERIFY(waitSignal(eventsCommitted));

    g.setId(-1);
    addTestGroup(g,ACCOUNT2,QString("td@localhost"));

    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, g.id());
    QVERIFY(waitSignal(eventsCommitted));

    g.setId(-1);
    addTestGroup(g,ACCOUNT2,QString("td2@localhost"));

    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, g.id());
    QVERIFY(waitSignal(eventsCommitted));
}

void GroupModelTest::cleanup()
{
    QTest::qWait(1000);

    deleteAll();
    QDir mms_content(QDir::homePath() + QDir::separator() + mms_content_path);
    mms_content.rmdir(mms_token1);
    mms_content.rmdir(mms_token2);
    mms_content.rmdir(mms_token3);
    //QTest::qWait(1000);
}

void GroupModelTest::addGroups()
{
    GroupModel model;
    EventModel eventModel;
    model.enableContactChanges(false);
    model.setQueryMode(EventModel::SyncQuery);

    /* add invalid group */
    QVERIFY(!model.addGroup(group1));

    addTestGroup(group1, ACCOUNT1, QString("td@localhost"));
    QVERIFY(group1.id() != -1);

    Group group;
    QVERIFY(model.databaseIO().getGroup(group1.id(), group));
    QCOMPARE(group.id(), group1.id());
    QCOMPARE(group.localUid(), group1.localUid());
    QCOMPARE(group.recipients(), group1.recipients());
    QCOMPARE(group.chatName(), group1.chatName());
    QVERIFY(group.startTime().isValid() == false);
    QVERIFY(group.endTime().isValid() == false);
    QCOMPARE(group.unreadMessages(), 0);
    QCOMPARE(group.lastEventId(), -1);

    // add an event to each group to get them to show up in getGroups()
    QSignalSpy eventsCommitted(&eventModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    addTestEvent(eventModel, Event::IMEvent, Event::Outbound, ACCOUNT1, group.id());
    QVERIFY(waitSignal(eventsCommitted));

    addTestGroup(group2, ACCOUNT1, QString("td2@localhost"));

    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT1, group2.id());
    QVERIFY(waitSignal(eventsCommitted));

    Group group3;
    addTestGroup(group3, ACCOUNT2, QString("td@localhost"));

    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, group3.id());
    QVERIFY(waitSignal(eventsCommitted));

    Group group4;
    addTestGroup(group4, ACCOUNT2, QString("td2@localhost"));

    addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, group4.id());
    QVERIFY(waitSignal(eventsCommitted));
}

void GroupModelTest::modifyGroup()
{
    GroupModel model;
    model.enableContactChanges(false);
    QSignalSpy groupsCommitted(&model, SIGNAL(groupsCommitted(QList<int>,bool)));

    Group group5;
    group5.setChatName("MUC topic");
    addTestGroup(group5, ACCOUNT1, QString("td2@localhost"));
    QVERIFY(group5.id() != -1);

    Group testGroupA;
    QVERIFY(model.databaseIO().getGroup(group5.id(), testGroupA));
    QCOMPARE(testGroupA.id(), group5.id());
    QCOMPARE(testGroupA.localUid(), group5.localUid());
    QCOMPARE(testGroupA.recipients(), group5.recipients());
    QCOMPARE(testGroupA.chatName(), group5.chatName());

    Group group6;
    QVERIFY(!model.modifyGroup(group6));

    group5.setChatName("MUC topic modified");
    QVERIFY(model.modifyGroup(group5));
    loop->exec();
    QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());

    QTRY_COMPARE(modifiedGroupId, group5.id());
    modifiedGroupId = -1;

    Group testGroupB;
    QVERIFY(model.databaseIO().getGroup(group5.id(), testGroupB));
    QCOMPARE(testGroupB.id(), group5.id());
    QCOMPARE(testGroupB.localUid(), group5.localUid());
    QCOMPARE(testGroupB.recipients(), group5.recipients());
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
    model.enableContactChanges(false);
    QSignalSpy groupsCommitted(&model, SIGNAL(groupsCommitted(QList<int>,bool)));

    QSignalSpy modelReady(&model, SIGNAL(modelReady(bool)));
    GroupModel listenerModel;
    listenerModel.enableContactChanges(false);

    QThread modelThread;
    if (useThread) {
        modelThread.start();
        model.setBackgroundThread(&modelThread);
    }

    listenerModel.setQueryMode(EventModel::SyncQuery);

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));
    QCOMPARE(model.rowCount(), 4);
    QVERIFY(listenerModel.getGroups());
    QCOMPARE(listenerModel.rowCount(), 4);

    /* add new group */
    Group group;
    group.setLocalUid(ACCOUNT2);
    group.setRecipients(RecipientList::fromUids(ACCOUNT2, QStringList() << "55501234567"));
    group.setId(-1);
    QVERIFY(model.addGroup(group));
    loop->exec();
    QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QCOMPARE(model.rowCount(), 5);
    QTRY_COMPARE(listenerModel.rowCount(), 5);

    /* filter by localUid */
    modelReady.clear();
    QVERIFY(model.getGroups(ACCOUNT1));
    QVERIFY(waitSignal(modelReady));
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(listenerModel.getGroups(ACCOUNT1));
    QCOMPARE(listenerModel.rowCount(), 2);

    /* filter out new group */
    group.setLocalUid(ACCOUNT2);
    group.setRecipients(RecipientList::fromUids(ACCOUNT2, QStringList() << "td@localhost"));
    group.setId(-1);
    groupsCommitted.clear();
    QVERIFY(model.addGroup(group));
    loop->exec();
    QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    /* add new group */
    group.setLocalUid(ACCOUNT1);
    group.setRecipients(RecipientList::fromUids(ACCOUNT1, QStringList() << "55566601234567"));
    group.setId(-1);
    groupsCommitted.clear();
    QVERIFY(model.addGroup(group));
    loop->exec();
    QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(listenerModel.rowCount(), 3);

    //QTest::qWait(1000);

    /* filter by localUid and IM remoteUid */
    modelReady.clear();
    QVERIFY(model.getGroups(ACCOUNT1, "td@localhost"));
    QVERIFY(waitSignal(modelReady));
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(listenerModel.getGroups(ACCOUNT1, "td@localhost"));
    QCOMPARE(listenerModel.rowCount(), 1);

    /* add new matching group */
    group.setLocalUid(ACCOUNT1);
    group.setRecipients(RecipientList::fromUids(ACCOUNT1, QStringList() << "td@localhost"));
    group.setId(-1);
    QVERIFY(model.addGroup(group));
    loop->exec();
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    /* filter out new group */
    group.setLocalUid(ACCOUNT1);
    group.setRecipients(RecipientList::fromUids(ACCOUNT1, QStringList() << "no@match"));
    group.setId(-1);
    groupsCommitted.clear();
    QVERIFY(model.addGroup(group));
    loop->exec();
    QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    /* filter by localUid and phone number remoteUid */
    modelReady.clear();
    QVERIFY(model.getGroups(ACCOUNT1, "55566601234567"));
    QVERIFY(waitSignal(modelReady));
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(listenerModel.getGroups(ACCOUNT1, "55566601234567"));
    QCOMPARE(listenerModel.rowCount(), 1);

    /* add new matching group */
    group.setLocalUid(ACCOUNT1);
    group.setRecipients(RecipientList::fromUids(ACCOUNT1, QStringList() << "55566601234567"));
    group.setId(-1);
    groupsCommitted.clear();
    QVERIFY(model.addGroup(group));
    loop->exec();
    QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    /* filter out new group */
    group.setLocalUid(ACCOUNT1);
    group.setRecipients(RecipientList::fromUids(ACCOUNT1, QStringList() << "+99966607654321"));
    group.setId(-1);
    groupsCommitted.clear();
    QVERIFY(model.addGroup(group));
    loop->exec();
    QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(listenerModel.rowCount(), 2);

    modelThread.quit();
    modelThread.wait(5000);
}

void GroupModelTest::updateGroups()
{
    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    groupModel.setQueryMode(EventModel::SyncQuery);
    QVERIFY(groupModel.getGroups(ACCOUNT1));
    connect(&groupModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)),
            this, SLOT(dataChangedSlot(const QModelIndex &, const QModelIndex &)));

    // update last event of group
    QTest::qWait(1000); // separate event from the rest
    EventModel model;
    QSignalSpy eventsCommitted(&model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QSignalSpy groupMoved(&groupModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    QSignalSpy groupChanged(&groupModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)));

    int eventId = addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                 groupModel.group(groupModel.index(0, 0)).id(), "added to group");
    QVERIFY(waitSignal(eventsCommitted));
    QVERIFY(waitSignal(groupChanged));
    Group group = groupModel.group(groupModel.index(0, 0));
    Event event;
    QVERIFY(group.lastEventId() != -1);
    QVERIFY(model.databaseIO().getEvent(eventId, event));
    QCOMPARE(group.lastEventId(), eventId);
    QCOMPARE(group.lastMessageText(), QString("added to group"));
    QCOMPARE(group.startTime().toTime_t(), event.startTime().toTime_t());
    QCOMPARE(group.endTime().toTime_t(), event.endTime().toTime_t());

    // add older event
    int lastEventId = eventId;
    uint lastStartTime = group.startTime().toTime_t();
    uint lastEndTime = group.endTime().toTime_t();
    eventsCommitted.clear();
    groupMoved.clear();
    eventId = addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1,
                 groupModel.group(groupModel.index(0, 0)).id(), "older added to group",
                 false, false, QDateTime::currentDateTime().addMonths(-1));
    QVERIFY(waitSignal(eventsCommitted));
    QVERIFY(waitSignal(groupChanged));
    group = groupModel.group(groupModel.index(0, 0));
    QVERIFY(group.lastEventId() != eventId);
    QVERIFY(model.databaseIO().getEvent(eventId, event));
    QCOMPARE(group.lastEventId(), lastEventId);
    QCOMPARE(group.lastMessageText(), QString("added to group"));
    QCOMPARE(group.startTime().toTime_t(), lastStartTime);
    QCOMPARE(group.endTime().toTime_t(), lastEndTime);

    // add new SMS event for second group, check for resorted list, correct contents and date
    QTest::qWait(1000);
    int id = groupModel.group(groupModel.index(1, 0)).id();
    QDateTime modified = groupModel.index(1, GroupModel::EndTime).data().toDateTime();
    eventsCommitted.clear();
    groupMoved.clear();
    addTestEvent(model, Event::SMSEvent, Event::Outbound, ACCOUNT1, id, "sms");
    QVERIFY(waitSignal(eventsCommitted));
    QVERIFY(waitSignal(groupMoved));
    group = groupModel.group(groupModel.index(0, 0));
    QCOMPARE(group.id(), id);
    QVERIFY(group.endTime() > modified);
    Group testGroup;
    QVERIFY(groupModel.databaseIO().getGroup(id, testGroup));
    QCOMPARE(testGroup.startTime().toTime_t(), group.startTime().toTime_t());
    QCOMPARE(testGroup.endTime().toTime_t(), group.endTime().toTime_t());

    // add new IM event for second group, check for resorted list, correct contents and date
    QTest::qWait(1000);
    id = groupModel.group(groupModel.index(1, 0)).id();
    modified = groupModel.index(1, GroupModel::EndTime).data().toDateTime();
    eventsCommitted.clear();
    groupMoved.clear();
    addTestEvent(model, Event::IMEvent, Event::Outbound, ACCOUNT1, id, "sort");
    QVERIFY(waitSignal(eventsCommitted));
    QVERIFY(waitSignal(groupMoved));
    group = groupModel.group(groupModel.index(0, 0));
    QCOMPARE(group.id(), id);
    QVERIFY(group.endTime() > modified);

    QVERIFY(groupModel.databaseIO().getGroup(id, testGroup));
    QCOMPARE(testGroup.startTime().toTime_t(), group.startTime().toTime_t());
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
    QVERIFY(model.databaseIO().getEvent(group.lastEventId(), event));
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

    qRegisterMetaType<QModelIndex>("QModelIndex");
    QSignalSpy groupsCommitted(&deleterModel, SIGNAL(groupsCommitted(QList<int>,bool)));
    QSignalSpy rowsRemoved(&groupModel, SIGNAL(rowsRemoved(QModelIndex,int,int)));

    groupModel.enableContactChanges(false);
    groupModel.setQueryMode(EventModel::SyncQuery);
    QVERIFY(groupModel.getGroups());
    int numGroups = groupModel.rowCount();

    // delete non-existing group
    deleterModel.deleteGroups(QList<int>() << -1);
    loop->exec();
    QCOMPARE(groupModel.rowCount(), numGroups);

    // delete first group
    messageId = groupModel.group(groupModel.index(0, 0)).lastEventId();
    deleterModel.deleteGroups(QList<int>() << groupModel.group(groupModel.index(0, 0)).id());
    loop->exec();
    QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QVERIFY(waitSignal(rowsRemoved));
    QCOMPARE(groupModel.rowCount(), numGroups - 1);
    QVERIFY(!model.databaseIO().getEvent(messageId, event));

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
    QVERIFY(waitSignal(groupDataChanged));
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
    QVERIFY(waitSignal(groupDataChanged));
    groupDataChanged.clear();

    groupsCommitted.clear();
    rowsRemoved.clear();
    deleterModel.deleteGroups(QList<int>() << groupModel.group(groupModel.index(0, 0)).id());
    loop->exec();
    QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QVERIFY(waitSignal(rowsRemoved));
    QCOMPARE(groupModel.rowCount(), numGroups - 2);

    QVERIFY(!model.databaseIO().getEvent(sms.id(), event));
    QVERIFY(!model.databaseIO().getEvent(mms.id(), event));
}

void GroupModelTest::streamingQuery_data()
{
    QTest::addColumn<bool>("useThread");

    QTest::newRow("Use thread") << true;
    QTest::newRow("Without thread") << false;
}

void GroupModelTest::streamingQuery()
{
    QSKIP("StreamedAsyncQuery is not yet supported with SQLite");
    QFETCH(bool, useThread);

    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    GroupModel streamModel;
    streamModel.enableContactChanges(false);
    QSignalSpy modelReady0(&groupModel, SIGNAL(modelReady(bool)));
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
        addTestGroup(group1, ACCOUNT1, QString("td@localhost"));
        addTestEvent(eventModel, Event::IMEvent, Event::Outbound, ACCOUNT1, group1.id());
        QVERIFY(waitSignal(eventsCommitted));
    }

    QTest::qWait(2000);
    QVERIFY(groupModel.getGroups());
    QVERIFY(waitSignal(modelReady0));
    QVERIFY(modelReady0.first().at(0).toBool());

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
    QSignalSpy modelReady(&streamModel, SIGNAL(modelReady(bool)));
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

        bool lastBatch = count + chunkSize >= total;
        int expectedEnd = lastBatch ? total -1 : count + chunkSize -1;
        int lastInserted = -1;
        int firstInserted = -1;

        while (expectedEnd != lastInserted) {
            QVERIFY(waitSignal(rowsInserted));

            if (firstInserted == -1)
                firstInserted = rowsInserted.first().at(1).toInt();
            lastInserted = rowsInserted.last().at(2).toInt(); // end

            qDebug() << "rows start in streaming model: " << firstInserted;
            qDebug() << "rows start should be: " << count;
            qDebug() << "rows end in streaming model: " << lastInserted;
            qDebug() << "rows end should be: " << expectedEnd;

            rowsInserted.clear();
        }

        QCOMPARE(firstInserted, count); // start

        for (int i = count; i < expectedEnd + 1; i++) {
            Group group1 = groupModel.group(groupModel.index(i, 0));
            Group group2 = streamModel.group(streamModel.index(i, 0));
            QCOMPARE(group1.startTime(),group2.startTime());
            QCOMPARE(group1.endTime(),group2.endTime());
            idsOrig.append(group1.id());
            idsStream.append(group2.id());
            //QCOMPARE(group1.id(), group2.id()); // Cannot compare like this, because groups having same timestamp
            //can be in random order in data model.
        }

        // You should be able to fetch more if total number of groups is not yet reached:
        if ( expectedEnd < total-1 )
        {
            QVERIFY(streamModel.canFetchMore(QModelIndex()));
            QVERIFY(modelReady.isEmpty());

            qDebug() << "Calling fetchMore from streaming model...";
            streamModel.fetchMore(QModelIndex());
        }

        count += chunkSize;
    }

    if (streamModel.canFetchMore(QModelIndex()))
        streamModel.fetchMore(QModelIndex());

    QVERIFY(waitSignal(modelReady));
    QVERIFY(!streamModel.canFetchMore(QModelIndex()));
    QCOMPARE(idsOrig.toSet().size(), idsOrig.size());
    QCOMPARE(idsStream.toSet().size(), idsStream.size());
    QVERIFY(idsOrig.toSet() == idsStream.toSet());

    modelThread.quit();
    modelThread.wait(5000);
}

void GroupModelTest::addMultipleGroups()
{
    deleteAll();

    QList<Group> groups;
    for (int i = 0; i < ADD_GROUPS_NUM; i++) {
        Group group;
        group.setLocalUid(ACCOUNT1);
        group.setRecipients(RecipientList::fromUids(ACCOUNT1, QStringList() << QString("testgroup%1").arg(i)));
        groups.append(group);
    }

    GroupModel model;
    model.enableContactChanges(false);
    model.setQueryMode(EventModel::SyncQuery);
    QSignalSpy groupsCommitted(&model, SIGNAL(groupsCommitted(QList<int>, bool)));
    QVERIFY(model.addGroups(groups));
    QList<int> committedIds;
    while (committedIds.count() < ADD_GROUPS_NUM) {
        QVERIFY(waitSignal(groupsCommitted));
        QVERIFY(groupsCommitted.first().at(1).toBool());
        committedIds << groupsCommitted.first().at(0).value<QList<int> >();
        groupsCommitted.clear();
    }
    QCOMPARE(committedIds.count(), ADD_GROUPS_NUM);
    QCOMPARE(model.rowCount(), ADD_GROUPS_NUM);

    QVERIFY(model.getGroups());
    QCOMPARE(model.rowCount(), ADD_GROUPS_NUM);

    for (int i = 0; i < ADD_GROUPS_NUM; i++) {
        QVERIFY(groups[i].id() != -1);
        QVERIFY(committedIds.contains(groups[i].id()));
        QVERIFY(committedIds.contains(model.group(model.index(i, 0)).id()));
    }
}

void GroupModelTest::limitOffset()
{
    GroupModel model;
    model.enableContactChanges(false);
    QSignalSpy modelReady(&model, SIGNAL(modelReady(bool)));

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));

    QCOMPARE(model.rowCount(), 4);
    for (int i = 0; i < 4; i++)
        qDebug() << model.group(model.index(i, 0)).id();

    model.setLimit(2);

    QTime timer;
    timer.start();
    while (timer.elapsed() < 2000)
        QCoreApplication::processEvents();

    modelReady.clear();
    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));

    QCOMPARE(model.rowCount(), 2);
    QSet<int> headGroups;
    headGroups.insert(model.group(model.index(0, 0)).id());
    headGroups.insert(model.group(model.index(1, 0)).id());

    qDebug() << model.group(model.index(0, 0)).toString();
    qDebug() << model.group(model.index(1, 0)).toString();

    model.setOffset(2);

    timer.start();
    while (timer.elapsed() < 2000)
        QCoreApplication::processEvents();

    modelReady.clear();
    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));

    QCOMPARE(model.rowCount(), 2);

    if (headGroups.contains(model.group(model.index(0, 0)).id())
        || headGroups.contains(model.group(model.index(1, 0)).id())) {
        qDebug() << model.group(model.index(0, 0)).toString();
        qDebug() << model.group(model.index(1, 0)).toString();
    }

    QVERIFY(!headGroups.contains(model.group(model.index(0, 0)).id()));
    QVERIFY(!headGroups.contains(model.group(model.index(1, 0)).id()));
}

void GroupModelTest::cleanupTestCase()
{
    deleteAll();
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
    Group group1, group2, group3;
    GroupModel model;
    model.enableContactChanges(false);
    EventModel eventModel;
    Event e;
    int id1, id2, id3;

    QDir content_dir(QDir::homePath() + QDir::separator() + mms_content_path);
    QVERIFY(content_dir.mkpath(mms_token1));
    QVERIFY(content_dir.mkdir(mms_token2));
    QVERIFY(content_dir.mkdir(mms_token3));

    model.setQueryMode(EventModel::SyncQuery);

    // Create 3 groups
    addTestGroup(group1, ACCOUNT1, QString("mms-test1@localhost"));
    QVERIFY(group1.id() != -1);

    addTestGroup(group2, ACCOUNT1, QString("mms-test2@localhost"));
    QVERIFY(group2.id() != -1);

    addTestGroup(group3, ACCOUNT1, QString("mms-test3@localhost"));
    QVERIFY(group3.id() != -1);

    // Populate groups
    // ev1 -> token1

    // ev2 -> token2
    // ev3 -> token2

    // ev4 -> token3
    // ev5 -> token3
    // ev6 -> token3

    QSignalSpy eventsCommitted(&eventModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    id1 = addTestMms(eventModel, Event::Inbound, ACCOUNT1, group1.id(), mms_token1, "test MMS to delete");
    QVERIFY(waitSignal(eventsCommitted));

    id2 = addTestMms(eventModel, Event::Outbound, ACCOUNT1, group2.id(), mms_token2, "test MMS to delete");
    QVERIFY(waitSignal(eventsCommitted));

    id3 = addTestMms(eventModel, Event::Outbound, ACCOUNT1, group3.id(), mms_token2, "test MMS to delete");
    QVERIFY(waitSignal(eventsCommitted));

    addTestMms(eventModel, Event::Inbound, ACCOUNT1, group1.id(), mms_token3, "test MMS to delete");
    QVERIFY(waitSignal(eventsCommitted));

    addTestMms(eventModel, Event::Inbound, ACCOUNT1, group2.id(), mms_token3, "test MMS to delete");
    QVERIFY(waitSignal(eventsCommitted));

    addTestMms(eventModel, Event::Inbound, ACCOUNT1, group3.id(), mms_token3, "test MMS to delete");
    QVERIFY(waitSignal(eventsCommitted));

    QVERIFY(model.databaseIO().getEvent(id1, e));
    QVERIFY(model.databaseIO().deleteEvent(e, 0));
    // Group deleting behavior is disabled currently, because it should be implemented in a
    // smarter and less synchronous way.
#if 0
    QTest::qWait(1000);
    QVERIFY(content_dir.exists(mms_token1) == false); // folder shall be removed since no more events reffers to the token
#endif

    QVERIFY(model.databaseIO().getEvent(id2, e));
    QVERIFY(model.databaseIO().deleteEvent(e, 0));
    QTest::qWait(1000);
    QVERIFY(content_dir.exists(mms_token2) == true);  // one more events refers to token2

    QVERIFY(model.databaseIO().getEvent(id3, e));
    QVERIFY(model.databaseIO().deleteEvent(e, 0));
    // Group deleting behvaior is disabled currently, because it should be implemented in a
    // smarter and less synchronous way.
#if 0
    QTest::qWait(1000);
    QVERIFY(content_dir.exists(mms_token2) == false);  // no more events with token2

    QSignalSpy groupsCommitted(&model, SIGNAL(groupsCommitted(QList<int>,bool)));
    model.deleteGroups(QList<int>() << group1.id());
    if (groupsCommitted.isEmpty())
        QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QVERIFY(content_dir.exists(mms_token3) == true);
    QTest::qWait(1000); // folders deleted after transaction, wait a bit

    groupsCommitted.clear();
    model.deleteGroups(QList<int>() << group1.id() << group2.id() << group3.id());
    if (groupsCommitted.isEmpty())
        QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    QTest::qWait(1000); // folders deleted after transaction, wait a bit

    QVERIFY(content_dir.exists(mms_token3) == false);
#endif
}

void GroupModelTest::markGroupAsRead()
{
    EventModel eventModel;
    GroupModel groupModel;
    QSignalSpy groupsCommitted(&groupModel, SIGNAL(groupsCommitted(QList<int>,bool)));
    groupModel.enableContactChanges(false);
    Group group;
    addTestGroup(group,ACCOUNT2,QString("td@localhost"));

    QSignalSpy eventsCommitted(&eventModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    // Test event is unread by default.
    int eventId1 = addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, group.id(), "Mark group as read test 1");
    QVERIFY(waitSignal(eventsCommitted));

    eventsCommitted.clear();
    int eventId2 = addTestEvent(eventModel, Event::IMEvent, Event::Inbound, ACCOUNT2, group.id(), "Mark group as read test 2");

    QVERIFY(groupModel.markAsReadGroup(group.id()));

    if (groupsCommitted.isEmpty())
        QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());

    Event e1, e2;
    QVERIFY(groupModel.databaseIO().getEvent(eventId1, e1));
    QVERIFY(e1.isRead());

    QVERIFY(groupModel.databaseIO().getEvent(eventId2, e2));
    QVERIFY(e2.isRead());

    Group testGroup;
    QVERIFY(groupModel.databaseIO().getGroup(group.id(), testGroup));
    QCOMPARE(testGroup.id(), group.id());
    QCOMPARE(testGroup.unreadMessages(),0);
}

void GroupModelTest::resolveContact()
{
    QSKIP("Contact matching is not yet supported with SQLite");
    GroupModel groupModel;

    QSignalSpy groupDataChanged(&groupModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)));
    QSignalSpy groupsCommitted(&groupModel, SIGNAL(groupsCommitted(QList<int>,bool)));

    QString phoneNumber = QString().setNum(qrand() % 10000000);
    QString contactName = QString("Test Contact 123");
//    int contactId = addTestContact(contactName, phoneNumber);
    addTestContact(contactName, phoneNumber);
    QTime timer;
    timer.start();
    while (timer.elapsed() < 2000)
        QCoreApplication::processEvents();

    Group grp;
    QStringList uids;
    uids << phoneNumber;
    grp.setLocalUid(RING_ACCOUNT);
    grp.setRecipients(RecipientList::fromUids(RING_ACCOUNT, uids));

    // CONTACT RESOLVING WHEN ADDING A GROUP:
    QVERIFY(groupModel.addGroup(grp));
    // Waiting for groupsAdded signal.
    loop->exec();
    if (groupsCommitted.isEmpty())
        QVERIFY(waitSignal(groupsCommitted));
    QVERIFY(groupsCommitted.first().at(1).toBool());
    // Waiting for dataChanged signal to update resolved contact name into the group.
    QVERIFY(waitSignal(groupDataChanged));

    QVERIFY(grp.id() != -1);

    // Check that tracker is updated:
    Group group;
    QVERIFY(groupModel.databaseIO().getGroup(grp.id(), group));
    QCOMPARE(group.id(), grp.id());
    QCOMPARE(group.localUid(), grp.localUid());
    QCOMPARE(group.recipients(), grp.recipients());
    QCOMPARE(group.recipients().containsMatch(Recipient(grp.localUid(), contactName)), true);

    // Check that group model is updated:
    group = groupModel.group(groupModel.index(0, 0));
    QCOMPARE(group.id(), grp.id());
    QCOMPARE(group.recipients().containsMatch(Recipient(grp.localUid(), contactName)), true);

    // FIXME: apparently this can fail in CITA.
    // something wrong with test contact handling when run after other tests?
#if 0
    // CHANGE CONTACT NAME:
    QString newName("Modified Test Contact 123");
    modifyTestContact(contactId, newName);

    // Waiting for dataChanged signal to update contact name into the group.
    groupDataChanged.clear();
    QVERIFY(waitSignal(groupDataChanged, 30000));

    // Check that group model is updated:
    group = groupModel.group(groupModel.index(0, 0));
    QCOMPARE(group.id(), grp.id());
    QCOMPARE(group.localUid(), grp.localUid());
    QCOMPARE(group.recipients(), grp.recipients());
    //QCOMPARE(group.recipients().containsMatch(Recipient(grp.localUid(), newName)), true);

    deleteTestContact(group.contactId());

    // Waiting for dataChanged signal to indicate that contact name has been removed from the group.
    groupDataChanged.clear();
    QVERIFY(waitSignal(groupDataChanged));

    // Check that group model is updated:
    group = groupModel.group(groupModel.index(0, 0));
    QCOMPARE(group.id(), grp.id());
    QCOMPARE(group.localUid(), grp.localUid());
    QCOMPARE(group.recipients(), grp.recipients());
    QCOMPARE(group.recipients().count(), 0);
#endif
}

void GroupModelTest::queryContacts()
{
    QSKIP("Contact matching is not yet supported with SQLite");
    GroupModel model;
    model.enableContactChanges(false);
    QSignalSpy modelReady(&model, SIGNAL(modelReady(bool)));

    //add cellular groups
    Group group;
    addTestGroup(group, RING_ACCOUNT, "445566");
    addTestGroup(group, RING_ACCOUNT, "+3854892930");

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));
    if (model.rowCount() != 6) {
        for (int i = 0; i < model.rowCount(); i++) {
            qDebug() << model.group(model.index(i, 0)).toString();
        }
    }
    QCOMPARE(model.rowCount(), 6);

    for (int i=0; i < model.rowCount(); i++) {
        Group g = model.group(model.index(i, 0));
        QCOMPARE(g.recipients().count(), 0);
    }

    int contactIdNoMatch = addTestContact("NoMatch",
                                          "nomatch",
                                          ACCOUNT1);

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));

    for (int i=0; i < model.rowCount(); i++) {
        Group g = model.group(model.index(i, 0));
        QCOMPARE(g.recipients().count(), 0);
    }

    int contactIdNoMatchPhone = addTestContact("PhoneNoMatch",
                                               "98765");

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));

    for (int i=0; i < model.rowCount(); i++) {
        Group g = model.group(model.index(i, 0));
        QCOMPARE(g.recipients().count(), 0);
    }

    int contactIdTD1 = addTestContact("TD1",
                                      "td@localhost",
                                      ACCOUNT1);

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));

    for (int i=0; i < model.rowCount(); i++) {
        Group g = model.group(model.index(i, 0));
        Recipient recipient(ACCOUNT1, "td@localhost");
        if (!recipient.isNull()) {
            QCOMPARE(g.recipients().count(), 1);
            QCOMPARE(g.recipients().at(0).contactId(), contactIdTD1);
            QCOMPARE(g.recipients().at(0).contactName(), QString("TD1"));
        } else {
            QCOMPARE(g.recipients().count(), 0);
        }
    }

    int contactIdTD2 = addTestContact("TD2",
                                      "td2@localhost",
                                      ACCOUNT2);

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));

    for (int i=0; i < model.rowCount(); i++) {
        Group g = model.group(model.index(i, 0));
        Recipient recipient1(ACCOUNT1, "td@localhost");
        Recipient recipient2(ACCOUNT2, "td2@localhost");
        if (!recipient1.isNull() && g.recipients().containsMatch(recipient1)) {
            QCOMPARE(g.recipients().count(), 1);
            QCOMPARE(g.recipients().at(0).contactId(), contactIdTD1);
            QCOMPARE(g.recipients().at(0).contactName(), QString("TD1"));
        } else if (!recipient2.isNull() && g.recipients().containsMatch(recipient2)) {
            QCOMPARE(g.recipients().count(), 1);
            QCOMPARE(g.recipients().at(0).contactId(), contactIdTD2);
            QCOMPARE(g.recipients().at(0).contactName(), QString("TD2"));
        } else {
            QCOMPARE(g.recipients().count(), 0);
        }
    }

    int contactIdPhone1 = addTestContact("CodeRed",
                                         "445566");

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));

    for (int i=0; i < model.rowCount(); i++) {
        Group g = model.group(model.index(i, 0));
        if (g.localUid() == RING_ACCOUNT) {
            Recipient recipient(RING_ACCOUNT, "445566");
            if (!recipient.isNull() && g.recipients().containsMatch(recipient)) {
                QCOMPARE(g.recipients().count(), 1);
                QCOMPARE(g.recipients().at(0).contactId(), contactIdPhone1);
                QCOMPARE(g.recipients().at(0).contactName(), QString("CodeRed"));
            } else {
                QCOMPARE(g.recipients().count(), 0);
            }
        }
    }

    int contactIdPhone2 = addTestContact("CodeBlue",
                                         "+3854892930");

    QVERIFY(model.getGroups());
    QVERIFY(waitSignal(modelReady));

    for (int i=0; i < model.rowCount(); i++) {
        Group g = model.group(model.index(i, 0));
        if (g.localUid() == RING_ACCOUNT) {
            Recipient recipient1(RING_ACCOUNT, "445566");
            Recipient recipient2(RING_ACCOUNT, "+3854892930");
            if (!recipient1.isNull() && g.recipients().containsMatch(recipient1)) {
                QCOMPARE(g.recipients().count(), 1);
                QCOMPARE(g.recipients().at(0).contactId(), contactIdPhone1);
                QCOMPARE(g.recipients().at(0).contactName(), QString("CodeRed"));
            } else if (!recipient2.isNull() && g.recipients().containsMatch(recipient2)) {
                QCOMPARE(g.recipients().count(), 1);
                QCOMPARE(g.recipients().at(0).contactId(), contactIdPhone1);
                QCOMPARE(g.recipients().at(0).contactName(), QString("CodeBlue"));
            } else {
                QCOMPARE(g.recipients().count(), 0);
            }
        }
    }

    deleteTestContact(contactIdNoMatchPhone);
    deleteTestContact(contactIdNoMatch);
    deleteTestContact(contactIdTD1);
    deleteTestContact(contactIdTD2);
    deleteTestContact(contactIdPhone1);
    deleteTestContact(contactIdPhone2);
}

void GroupModelTest::changeRemoteUid()
{
    QSKIP("Contact matching is not yet supported with SQLite");
    const QString oldRemoteUid = "+1117654321";
    const QString newRemoteUid = "+2227654321";
    bool oldContactFound = false;
    bool newContactFound = false;

    // Add two contacts with matching numbers
    int oldContactId = addTestContact("OldContact", oldRemoteUid, RING_ACCOUNT);
    int newContactId = addTestContact("NewContact", newRemoteUid, RING_ACCOUNT);
    qDebug() << "******************** oldContactId" << oldContactId << "new" << newContactId;

    QTime timer;
    timer.start();
    while (timer.elapsed() < 2000)
        QCoreApplication::processEvents();

    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    groupModel.setQueryMode(EventModel::SyncQuery);
    QSignalSpy modelReady(&groupModel, SIGNAL(modelReady(bool)));
    QVERIFY(groupModel.getGroups());
    QVERIFY(waitSignal(modelReady));

    // Add new group with old remoteUid
    Group group;
    addTestGroup(group, RING_ACCOUNT, oldRemoteUid);
    QVERIFY(groupModel.getGroups());
    QVERIFY(waitSignal(modelReady));

    int numGroups = groupModel.rowCount();
    int groupsFound = 0;
    for (int i=0; i < groupModel.rowCount(); i++) {
        Group g = groupModel.group(groupModel.index(i, 0));
        if (g.id() == group.id()) {
            QCOMPARE(g.recipients().size(), 1);
            QCOMPARE(g.recipients().containsMatch(Recipient(RING_ACCOUNT, oldRemoteUid)), true);
            QCOMPARE(g.contacts().size(), 2);
            oldContactFound = false;
            newContactFound = false;
            for (int j = 0; j < g.contacts().size(); j++) {
                Event::Contact c = g.contacts().at(j);
                if (c.first == oldContactId)
                    oldContactFound = true;
                if (c.first == newContactId)
                    newContactFound = true;
            }
            QVERIFY(oldContactFound);
            QVERIFY(newContactFound);
            groupsFound++;
        }
    }
    QCOMPARE(groupsFound, 1);

    // Add test event, which updates group remoteUid
    EventModel eventModel;
    QSignalSpy groupUpdated(&groupModel,
                            SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)));
    addTestEvent(eventModel, Event::SMSEvent, Event::Inbound, RING_ACCOUNT,
                 group.id(), "added to group", false, false,
                 QDateTime::currentDateTime(), newRemoteUid);
    QVERIFY(waitSignal(groupUpdated));

    timer.start();
    while (timer.elapsed() < 2000)
        QCoreApplication::processEvents();

    group = groupModel.group(groupModel.index(0, 0));
    QCOMPARE(group.recipients().size(), 1);
    QCOMPARE(group.recipients().containsMatch(Recipient(RING_ACCOUNT, newRemoteUid)), true);
    QCOMPARE(group.contacts().size(), 2);
    oldContactFound = false;
    newContactFound = false;
    for (int i = 0; i < group.contacts().size(); i++) {
        Event::Contact c = group.contacts().at(i);
        if (c.first == oldContactId)
            oldContactFound = true;
        if (c.first == newContactId)
            newContactFound = true;
    }
    QVERIFY(oldContactFound);
    QVERIFY(newContactFound);

    QVERIFY(groupModel.getGroups());
    QVERIFY(waitSignal(modelReady));
    QCOMPARE(groupModel.rowCount(), numGroups);
    group = groupModel.group(groupModel.index(0, 0));
    QCOMPARE(group.recipients().size(), 1);
    QCOMPARE(group.recipients().containsMatch(Recipient(RING_ACCOUNT, newRemoteUid)), true);
    QCOMPARE(group.contacts().size(), 2);
    oldContactFound = false;
    newContactFound = false;
    for (int i = 0; i < group.contacts().size(); i++) {
        Event::Contact c = group.contacts().at(i);
        if (c.first == oldContactId)
            oldContactFound = true;
        if (c.first == newContactId)
            newContactFound = true;
    }
    QVERIFY(oldContactFound);
    QVERIFY(newContactFound);

    deleteTestContact(oldContactId);
    deleteTestContact(newContactId);
}

void GroupModelTest::noRemoteId()
{
    GroupModel model;
    model.enableContactChanges(false);
    model.setQueryMode(EventModel::SyncQuery);
    Group groupR;

    groupR.setLocalUid(ACCOUNT1);
    QVERIFY(!model.addGroup(groupR));
}

void GroupModelTest::endTimeUpdate()
{
    GroupModel model;
    EventModel eventModel;
    model.enableContactChanges(false);
    model.setQueryMode(EventModel::SyncQuery);

    addTestGroup(group1, "endTimeUpdate", QString("td@localhost"));
    QVERIFY(group1.id() != -1);

    QSignalSpy modelReady(&model, SIGNAL(modelReady(bool)));
    QVERIFY(model.getGroups("endTimeUpdate"));
    QVERIFY(waitSignal(modelReady));

    QSignalSpy groupUpdated(&model,
                            SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)));

    QDateTime latestDate = QDateTime::currentDateTime().addMonths(-1);
    QDateTime oldDate = latestDate.addMonths(-1);

    // add an event to each group to get them to show up in getGroups()
    QSignalSpy eventsCommitted(&eventModel, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    int latestEventId = addTestEvent(eventModel, Event::IMEvent, Event::Outbound, "endTimeUpdate",
                                     group1.id(), "latest event",
                                     false, false, latestDate);

    QVERIFY(waitSignal(eventsCommitted));

    if (groupUpdated.isEmpty())
        QVERIFY(waitSignal(groupUpdated));
    QTest::qWait(1000);

    // verify run-time update
    QVERIFY(!groupUpdated.isEmpty());
    QCOMPARE(model.group(model.index(0, 0)).startTime().toTime_t(), latestDate.toTime_t());
    QCOMPARE(model.group(model.index(0, 0)).endTime().toTime_t(), latestDate.toTime_t());

    // and database update
    modelReady.clear();
    QVERIFY(model.getGroups("endTimeUpdate"));
    QVERIFY(waitSignal(modelReady));
    QCOMPARE(model.group(model.index(0, 0)).startTime().toTime_t(), latestDate.toTime_t());
    QCOMPARE(model.group(model.index(0, 0)).endTime().toTime_t(), latestDate.toTime_t());

    // add an event to each group to get them to show up in getGroups()
    eventsCommitted.clear();
    addTestEvent(eventModel, Event::IMEvent, Event::Outbound, "endTimeUpdate",
                 group1.id(), "old event",
                 false, false, oldDate);

    QVERIFY(waitSignal(eventsCommitted));

    if (groupUpdated.isEmpty())
        QVERIFY(waitSignal(groupUpdated));
    QTest::qWait(1000);

    // verify run-time update
    QVERIFY(!groupUpdated.isEmpty());
    QCOMPARE(model.group(model.index(0, 0)).lastEventId(), latestEventId);
    QCOMPARE(model.group(model.index(0, 0)).startTime().toTime_t(), latestDate.toTime_t());
    QCOMPARE(model.group(model.index(0, 0)).endTime().toTime_t(), latestDate.toTime_t());

    // and database update
    modelReady.clear();
    QVERIFY(model.getGroups("endTimeUpdate"));
    QVERIFY(waitSignal(modelReady));
    QCOMPARE(model.group(model.index(0, 0)).lastEventId(), latestEventId);
    QCOMPARE(model.group(model.index(0, 0)).startTime().toTime_t(), latestDate.toTime_t());
    QCOMPARE(model.group(model.index(0, 0)).endTime().toTime_t(), latestDate.toTime_t());

    // check group updates on modify event
    Event event;
    QVERIFY(model.databaseIO().getEvent(latestEventId, event));
    QDateTime latestestDate = event.startTime().addDays(5);
    event.setStartTime(latestestDate);
    event.setEndTime(latestestDate);

    eventsCommitted.clear();
    eventModel.modifyEvent(event);

    QVERIFY(waitSignal(eventsCommitted));
    modelReady.clear();
    QVERIFY(model.getGroups("endTimeUpdate"));
    QVERIFY(waitSignal(modelReady));
    QCOMPARE(model.group(model.index(0, 0)).lastEventId(), latestEventId);
    QCOMPARE(model.group(model.index(0, 0)).startTime().toTime_t(), latestestDate.toTime_t());
    QCOMPARE(model.group(model.index(0, 0)).endTime().toTime_t(), latestestDate.toTime_t());

    // check group updates on delete event
    eventsCommitted.clear();
    eventModel.deleteEvent(latestEventId);

    QVERIFY(waitSignal(eventsCommitted));
    modelReady.clear();
    QVERIFY(model.getGroups("endTimeUpdate"));
    QVERIFY(waitSignal(modelReady));
    QCOMPARE(model.group(model.index(0, 0)).startTime().toTime_t(), oldDate.toTime_t());
    QCOMPARE(model.group(model.index(0, 0)).endTime().toTime_t(), oldDate.toTime_t());

    // add overlapping event with receive time older than oldDate but the newest sent time
    // the event should not be picked up as the last event because of the old receive time
    Event olEvent;
    olEvent.setType(Event::IMEvent);
    olEvent.setDirection(Event::Outbound);
    olEvent.setGroupId(group1.id());
    olEvent.setStartTime(QDateTime::currentDateTime());
    olEvent.setEndTime(oldDate.addDays(-10));
    olEvent.setLocalUid("endTimeUpdate");
    olEvent.setRemoteUid("td@localhost");
    olEvent.setFreeText("long in delivery message");

    eventsCommitted.clear();
    eventModel.addEvent(olEvent);

    QVERIFY(waitSignal(eventsCommitted));

    if (groupUpdated.isEmpty())
        QVERIFY(waitSignal(groupUpdated));
    QTest::qWait(1000);

    // verify run-time update
    QVERIFY(!groupUpdated.isEmpty());
    QVERIFY(model.group(model.index(0, 0)).lastEventId() != olEvent.id());
    QVERIFY(model.group(model.index(0, 0)).startTime().toTime_t() != olEvent.startTime().toTime_t());
    QVERIFY(model.group(model.index(0, 0)).endTime().toTime_t() != olEvent.endTime().toTime_t());

    // and tracker update
    modelReady.clear();
    QVERIFY(model.getGroups("endTimeUpdate"));
    QVERIFY(waitSignal(modelReady));
    QVERIFY(model.group(model.index(0, 0)).lastEventId() != olEvent.id());
    QVERIFY(model.group(model.index(0, 0)).startTime().toTime_t() != olEvent.startTime().toTime_t());
    QVERIFY(model.group(model.index(0, 0)).endTime().toTime_t() != olEvent.endTime().toTime_t());
}

QTEST_MAIN(GroupModelTest)
