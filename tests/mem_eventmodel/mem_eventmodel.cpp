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
#include <time.h>
#include <malloc.h>
#include "eventmodel.h"
#include "callmodel.h"
#include "common.h"

#include "mem_eventmodel.h"

using namespace CommHistory;

Group group;

#define WAIT_TIMEOUT 30000
#define CALM_TIMEOUT 500

#define MALLINFO_DUMP(s) {struct mallinfo m = mallinfo();qDebug() << "MALLINFO" << (s) << m.arena << m.uordblks << m.fordblks;}

void MemEventModelTest::initTestCase()
{
    MALLINFO_DUMP("INIT");

    addTestGroup(group, "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0", "td@localhost");
    QTest::qWait(CALM_TIMEOUT);

    MALLINFO_DUMP("GROUP ADDED");
}

void MemEventModelTest::addEvent()
{
    MALLINFO_DUMP("start");

    EventModel *model = new EventModel();

    MALLINFO_DUMP("model ready");

    Event e1;
    e1.setGroupId(group.id());
    e1.setType(Event::IMEvent);
    e1.setDirection(Event::Outbound);
    e1.setStartTime(QDateTime::fromString("2010-01-08T13:37:00Z", Qt::ISODate));
    e1.setEndTime(QDateTime::fromString("2010-01-08T13:37:00Z", Qt::ISODate));
    e1.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    e1.setRecipients(Recipient(e1.localUid(), "td@localhost"));
    e1.setFreeText("addEvents 1");

    QVERIFY(model->addEvent(e1));
    QSignalSpy eventsCommitted(model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));

    QVERIFY(waitSignal(eventsCommitted, WAIT_TIMEOUT));

    QTest::qWait(CALM_TIMEOUT);

    MALLINFO_DUMP("query done");

    delete model;

    MALLINFO_DUMP("done");
}

void MemEventModelTest::addEvents()
{
    MALLINFO_DUMP("start");

    MALLINFO_DUMP("model ready");
    int mem=0;
    int lastMem=0;

    for (int i = 0; i < 100; i++) {
        EventModel *model = new EventModel();
        Event e1;

        e1.setGroupId(group.id());
        e1.setType(Event::IMEvent);
        e1.setDirection(Event::Outbound);
        e1.setStartTime(QDateTime::fromString("2010-01-08T13:37:00Z", Qt::ISODate));
        e1.setEndTime(QDateTime::fromString("2010-01-08T13:37:00Z", Qt::ISODate));
        e1.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
        e1.setRecipients(Recipient(e1.localUid(), "td@localhost"));
        e1.setFreeText(QString("addEvents %1").arg(i));

        QVERIFY(model->addEvent(e1));
        QSignalSpy eventsCommitted(model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));

        QVERIFY(waitSignal(eventsCommitted, WAIT_TIMEOUT));
        waitWithDeletes(100);

        delete model;

        waitWithDeletes(100);
        MALLINFO_DUMP("query done");
        struct mallinfo m = mallinfo();

        if (i >= 50)
            mem += m.uordblks - lastMem;
        lastMem = m.uordblks;
    }

    QTest::qWait(CALM_TIMEOUT);

    MALLINFO_DUMP("ALL DONE");

    qDebug() << "AVG LEAK" << mem/50;

    MALLINFO_DUMP("done");
}

void MemEventModelTest::modifyEvent()
{
    EventModel *model = new EventModel();

    Event im;
    im.setType(Event::IMEvent);
    im.setDirection(Event::Outbound);
    im.setGroupId(group.id());
    im.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    im.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    im.setRecipients(Recipient(im.localUid(), "td@localhost"));
    im.setFreeText("imtest");

    QSignalSpy eventsCommitted(model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(model->addEvent(im));

    QVERIFY(waitSignal(eventsCommitted, WAIT_TIMEOUT));

    im.resetModifiedProperties();
    im.setFreeText("imtest \"q\" modified\t tabs");
    im.setStartTime(QDateTime::currentDateTime());
    im.setEndTime(QDateTime::currentDateTime());
    im.setIsRead(false);
    // should we actually test more properties?
    QVERIFY(model->modifyEvent(im));
    QVERIFY(waitSignal(eventsCommitted, WAIT_TIMEOUT));

    QTest::qWait(CALM_TIMEOUT);

    delete model;
}

void MemEventModelTest::deleteEvent()
{
    EventModel *model = new EventModel();

    Event event;

    event.setType(Event::IMEvent);
    event.setDirection(Event::Inbound);
    event.setGroupId(group.id());
    event.setStartTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event.setEndTime(QDateTime::fromString("2009-08-26T09:37:47Z", Qt::ISODate));
    event.setLocalUid("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0");
    event.setRecipients(Recipient(event.localUid(), "td@localhost"));
    event.setFreeText("deletetest");

    QSignalSpy eventsCommitted(model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));

    QVERIFY(model->addEvent(event));
    QVERIFY(waitSignal(eventsCommitted, WAIT_TIMEOUT));

    QVERIFY(model->deleteEvent(event));
    QVERIFY(waitSignal(eventsCommitted, WAIT_TIMEOUT));

    QTest::qWait(CALM_TIMEOUT);

    delete model;
}

void MemEventModelTest::callSetFilter()
{
    MALLINFO_DUMP("start");

    CallModel *model = new CallModel();
    model->enableContactChanges(false);
    QSignalSpy ready(model, SIGNAL(modelReady(bool)));

    model->getEvents();
    ready.clear();
    QVERIFY(waitSignal(ready, WAIT_TIMEOUT));

    for(int i = 0; i < 5; i++) {

        if (i&1)
            model->setFilter(CallModel::SortByTime, CommHistory::CallEvent::MissedCallType);
        else
            model->setFilter(CallModel::SortByContact, CommHistory::CallEvent::UnknownCallType);
        ready.clear();
        QVERIFY(waitSignal(ready, WAIT_TIMEOUT));
        QTest::qWait(100);

        MALLINFO_DUMP("get");
    }
    delete model;
    MALLINFO_DUMP("del");
    QTest::qWait(CALM_TIMEOUT);
    MALLINFO_DUMP("don");
}

void MemEventModelTest::cleanupTestCase()
{
    MALLINFO_DUMP("CLEANUP");
}

QTEST_MAIN(MemEventModelTest)
