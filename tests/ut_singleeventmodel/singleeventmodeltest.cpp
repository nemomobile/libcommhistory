#include <QtTest/QtTest>
#include <QtTracker/Tracker>

#include <time.h>
#include "singleeventmodeltest.h"
#include "singleeventmodel.h"
#include "adaptor.h"
#include "event.h"
#include "common.h"
#include "trackerio.h"

#include "modelwatcher.h"

using namespace CommHistory;

Group group1, group2;
QEventLoop loop;

ModelWatcher watcher;

void SingleEventModelTest::initTestCase()
{
    deleteAll();

    qsrand(QDateTime::currentDateTime().toTime_t());

    watcher.setLoop(&loop);

    addTestGroups(group1, group2);
}

void SingleEventModelTest::getEventByUri()
{
    SingleEventModel model;

    watcher.setModel(&model);

    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Outbound);
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setGroupId(group1.id());
    event.setFreeText("freeText");
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setRemoteUid("123456");
    event.setMessageToken("messageToken");

    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();

    QVERIFY(event.id() != -1);
    QVERIFY(model.getEventByUri(event.url()));
    QVERIFY(watcher.waitForModelReady(5000));

    QCOMPARE(model.rowCount(), 1);

    Event modelEvent = model.event(model.index(0, 0));
    QVERIFY(compareEvents(event, modelEvent));
}

void SingleEventModelTest::getEventByTokens()
{
    SingleEventModel model;

    watcher.setModel(&model);

    Event event;
    event.setType(Event::SMSEvent);
    event.setDirection(Event::Outbound);
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setGroupId(group1.id());
    event.setFreeText("freeText");
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setRemoteUid("123456");
    event.setMessageToken("messageToken");

    QVERIFY(model.addEvent(event));
    watcher.waitForSignals();
    QVERIFY(event.id() != -1);

    Event mms(event);
    mms.setMessageToken("mmsMessageToken");
    mms.setMmsId("mmsId");

    QVERIFY(model.addEvent(mms));
    watcher.waitForSignals();
    QVERIFY(mms.id() != -1);

    Event mms2(event);
    mms2.setMessageToken("mmsMessageToken");
    mms2.setMmsId("mmsId");
    mms2.setGroupId(group2.id());

    QVERIFY(model.addEvent(mms2));
    watcher.waitForSignals();
    QVERIFY(mms2.id() != -1);

    QVERIFY(model.getEventByTokens("messageToken", "", -1));
    QVERIFY(watcher.waitForModelReady(5000));

    // Take into account the event with "messageToken" added already in getEventByUri test:
    QCOMPARE(model.rowCount(), 2);

    Event modelEvent = model.event(model.index(0, 0));
    QVERIFY(compareEvents(event, modelEvent));

    QVERIFY(model.getEventByTokens("messageToken", "", group1.id()));
    QVERIFY(watcher.waitForModelReady(5000));

    // Take into account the event with "messageToken" added already in getEventByUri test:
    QCOMPARE(model.rowCount(), 2);

    modelEvent = model.event(model.index(0, 0));
    QVERIFY(compareEvents(event, modelEvent));

    QVERIFY(model.getEventByTokens("messageToken", "", group1.id() + 1));
    QVERIFY(watcher.waitForModelReady(5000));

    QCOMPARE(model.rowCount(), 0);

    // Can match either to token or mms id:
    QVERIFY(model.getEventByTokens("messageToken", "nonExistingMmsId", group1.id()));
    QVERIFY(watcher.waitForModelReady(5000));

    // Take into account the event with "messageToken" added already in getEventByUri test:
    QCOMPARE(model.rowCount(), 2);

    modelEvent = model.event(model.index(0, 0));
    QVERIFY(compareEvents(event, modelEvent));

    QVERIFY(model.getEventByTokens("", "nonExistingMmsId", group1.id()));
    QVERIFY(watcher.waitForModelReady(5000));

    QCOMPARE(model.rowCount(), 0);

    QVERIFY(model.getEventByTokens("mmsMessageToken", "", -1));
    QVERIFY(watcher.waitForModelReady(5000));

    QCOMPARE(model.rowCount(), 2);

    QVERIFY(model.getEventByTokens("", "mmsId", group1.id()));
    QVERIFY(watcher.waitForModelReady(5000));

    QCOMPARE(model.rowCount(), 1);

    modelEvent = model.event(model.index(0, 0));
    QVERIFY(compareEvents(mms, modelEvent));

    QVERIFY(model.getEventByTokens("", "mmsId", -1));
    QVERIFY(watcher.waitForModelReady(5000));

    QCOMPARE(model.rowCount(), 2);

    QVERIFY(model.getEventByTokens("", "mmsId", group2.id()));
    QVERIFY(watcher.waitForModelReady(5000));

    QCOMPARE(model.rowCount(), 1);

    modelEvent = model.event(model.index(0, 0));
    QVERIFY(compareEvents(mms2, modelEvent));

    QVERIFY(model.getEventByTokens("mmsMessageToken", "mmsId", group1.id()));
    QVERIFY(watcher.waitForModelReady(5000));

    QCOMPARE(model.rowCount(), 1);

    modelEvent = model.event(model.index(0, 0));
    QVERIFY(compareEvents(mms, modelEvent));
}

void SingleEventModelTest::cleanupTestCase()
{
}

QTEST_MAIN(SingleEventModelTest)
