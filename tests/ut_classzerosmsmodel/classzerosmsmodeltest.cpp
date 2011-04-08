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
#include <time.h>
#include <QProcess>

#include "classzerosmsmodeltest.h"
#include "classzerosmsmodel.h"
#include "event.h"
#include "modelwatcher.h"

using namespace CommHistory;

#define NUM_EVENTS 5

const char *textContent[] = {
    "Earthquake, evac evac.",
    "It was a dark and stormy night.",
    "Bring it on."
};

ModelWatcher watcher;

ClassZeroSMSModelTest::~ClassZeroSMSModelTest()
{
    delete m_pModel;
}

Event createEvent()
{
    Event e;
    e.setType(Event::ClassZeroSMSEvent);
    e.setEndTime(QDateTime::currentDateTime());
    e.setStartTime(QDateTime::currentDateTime());
    e.setLocalUid("ring/tel/ring");
    e.setRemoteUid("16400");
    e.setFreeText(textContent[qrand() % 3]);
    e.setDirection(Event::Inbound);
    e.setIsRead(false);
    return e;
}

void ClassZeroSMSModelTest::initTestCase()
{
    m_pModel = new ClassZeroSMSModel;
    qsrand(QDateTime::currentDateTime().toTime_t());

    watcher.setLoop(&m_eventLoop);
    watcher.setModel(m_pModel);
}

void ClassZeroSMSModelTest::addEvent()
{
    for (int j = 0; j < NUM_EVENTS; j++) {
        Event e;
        int id;
        e = createEvent();
        QVERIFY(m_pModel->addEvent(e,true));
        id = e.id();
        QVERIFY(id != -1);
        watcher.waitForSignals(-1, 1);
        QCOMPARE(watcher.addedCount(), 1);
    }
}

void ClassZeroSMSModelTest::getEvents()
{
    const int count = m_pModel->rowCount();
    for(int i = 0; i < count; i++){
        Event event = m_pModel->event(m_pModel->index(i, 0));
        QVERIFY(event.type() == Event::ClassZeroSMSEvent);
    }
}

void ClassZeroSMSModelTest::deleteEvents()
{
    while(m_pModel->rowCount()) {
        Event event = m_pModel->event(m_pModel->index(0, 0));
        QVERIFY(event.id() != -1);
        m_pModel->deleteEvent(event.id());
        watcher.waitForSignals(-1, 1);
    }
    QVERIFY( m_pModel->rowCount() == 0);
}

void ClassZeroSMSModelTest::cleanupTestCase()
{
    QProcess::execute("pkill messaging-ui");
}

QTEST_MAIN(ClassZeroSMSModelTest)
