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
#include <QDateTime>
#include <QDBusConnection>
#include <cstdlib>
#include "callmodelperftest.h"
#include "callmodel.h"
#include "common.h"

using namespace CommHistory;

void CallModelPerfTest::initTestCase()
{
    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    qsrand( QDateTime::currentDateTime().toTime_t() );

    deleteAll();
}

void CallModelPerfTest::init()
{
}

void CallModelPerfTest::getEvents_data()
{
    QTest::addColumn<int>("events");
    QTest::addColumn<int>("contacts");
    QTest::addColumn<int>("selected");
    QTest::addColumn<bool>("resolve");

    QTest::newRow("10 events, 1 of 3 contacts") << 10 << 3 << 1 << false;
    QTest::newRow("10 events, 1 of 3 contacts with resolve") << 10 << 3 << 1 << true;
    QTest::newRow("100 events, 1 of 3 contacts") << 100 << 3 << 1 << false;
    QTest::newRow("100 events, 1 of 3 contacts with resolve") << 100 << 3 << 1 << true;
    QTest::newRow("1000 events, 1 of 3 contacts") << 1000 << 3 << 1 << false;
    QTest::newRow("1000 events, 1 of 3 contacts with resolve") << 1000 << 3 << 1 << true;
    QTest::newRow("10 events, 3 of 3 contacts") << 10 << 3 << 3 << false;
    QTest::newRow("10 events, 3 of 3 contacts with resolve") << 10 << 3 << 3 << true;
    QTest::newRow("100 events, 3 of 3 contacts") << 100 << 3 << 3 << false;
    QTest::newRow("100 events, 3 of 3 contacts with resolve") << 100 << 3 << 3 << true;
    QTest::newRow("1000 events, 3 of 3 contacts") << 1000 << 3 << 3 << false;
    QTest::newRow("1000 events, 3 of 3 contacts with resolve") << 1000 << 3 << 3 << true;
    QTest::newRow("10 events, 1 of 300 contacts") << 10 << 300 << 1 << false;
    QTest::newRow("10 events, 1 of 300 contacts with resolve") << 10 << 300 << 1 << true;
    QTest::newRow("100 events, 1 of 300 contacts") << 100 << 300 << 1 << false;
    QTest::newRow("100 events, 1 of 300 contacts with resolve") << 100 << 300 << 1 << true;
    QTest::newRow("1000 events, 1 of 300 contacts") << 1000 << 300 << 1 << false;
    QTest::newRow("1000 events, 1 of 300 contacts with resolve") << 1000 << 300 << 1 << true;
    QTest::newRow("10 events, 300 of 300 contacts") << 10 << 300 << 300 << false;
    QTest::newRow("10 events, 300 of 300 contacts with resolve") << 10 << 300 << 300 << true;
    QTest::newRow("100 events, 300 of 300 contacts") << 100 << 300 << 300 << false;
    QTest::newRow("100 events, 300 of 300 contacts with resolve") << 100 << 300 << 300 << true;
    QTest::newRow("1000 events, 300 of 300 contacts") << 1000 << 300 << 300 << false;
    QTest::newRow("1000 events, 300 of 300 contacts with resolve") << 1000 << 300 << 300 << true;
}

void CallModelPerfTest::getEvents()
{
    QFETCH(int, events);
    QFETCH(int, contacts);
    QFETCH(int, selected);
    QFETCH(bool, resolve);

    QDateTime startTime = QDateTime::currentDateTime();

    cleanupTestGroups();
    cleanupTestEvents();

    int commitBatchSize = 75;
    #ifdef PERF_BATCH_SIZE
    commitBatchSize = PERF_BATCH_SIZE;
    #endif

    EventModel addModel;
    QDateTime when = QDateTime::currentDateTime();

    qDebug() << __FUNCTION__ << "- Creating" << contacts << "contacts";

    QList<QPair<QString, QPair<QString, QString> > > contactDetails;

    int ci = remoteUids.count();
    while(ci < contacts) {
        QString phoneNumber;
        do {
            phoneNumber = QString().setNum(qrand() % 10000000);
        } while (remoteUids.contains(phoneNumber));
        remoteUids << phoneNumber;
        contactIndices << ci;
        ci++;

        contactDetails.append(qMakePair(QString("Test Contact %1").arg(ci), qMakePair(phoneNumber, QString())));

        if(ci % commitBatchSize == 0 && ci < contacts) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "contacts (" << ci << "/" << contacts << ")";
            addTestContacts(contactDetails);
            contactDetails.clear();
        }
    }
    if (!contactDetails.isEmpty()) {
        qDebug() << __FUNCTION__ << "- adding rest of the contacts ("
            << ci << "/" << contacts << ")";
        addTestContacts(contactDetails);
        contactDetails.clear();
    }

    qDebug() << __FUNCTION__ << "- Creating" << events << "new events";

    QList<Event> eventList;

    // Randomize the contact indices
    random_shuffle(contactIndices.begin(), contactIndices.end());

    int ei = 0;
    while(ei < events) {
        ei++;

        Event::EventDirection direction;
        bool isMissed = false;

        if(qrand() % 2 > 0) {
            direction = Event::Inbound;
            isMissed = (qrand() % 2 > 0);
        } else {
            direction = Event::Outbound;
        }

        Event e;
        e.setType(Event::CallEvent);
        e.setDirection(direction);
        e.setGroupId(-1);
        e.setStartTime(when.addSecs(ei));
        e.setEndTime(when.addSecs(ei));
        e.setLocalUid(RING_ACCOUNT);
        e.setRecipients(Recipient(RING_ACCOUNT, remoteUids.at(contactIndices.at(qrand() % selected))));
        e.setFreeText("");
        e.setIsDraft(false);
        e.setIsMissedCall(isMissed);

        eventList << e;

        if(ei % commitBatchSize == 0 && ei != events) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "events (" << ei << "/" << events << ")";
            QVERIFY(addModel.addEvents(eventList, false));
            eventList.clear();
        }
    }

    QVERIFY(addModel.addEvents(eventList, false));
    qDebug() << __FUNCTION__ << "- adding rest of the events ("
        << ei << "/" << events << ")";
    eventList.clear();

    QList<int> times;

    int iterations = 10;
    #ifdef PERF_ITERATIONS
    iterations = PERF_ITERATIONS;
    #endif

    char *iterVar = getenv("PERF_ITERATIONS");
    if (iterVar) {
        int iters = QString::fromLatin1(iterVar).toInt();
        if (iters > 0) {
            iterations = iters;
        }
    }

    qDebug() << __FUNCTION__ << "- Fetching events." << iterations << "iterations";
    for(int i = 0; i < iterations; i++) {

        CallModel fetchModel;

        fetchModel.setResolveContacts(resolve ? EventModel::ResolveImmediately : EventModel::DoNotResolve);
        fetchModel.setFilter(CallModel::SortByContact);

        waitForIdle();

        QTime time;
        time.start();

        bool result = fetchModel.getEvents();
        QVERIFY(result);

        if (!fetchModel.isReady())
            waitForSignal(&fetchModel, SIGNAL(modelReady(bool)));

        int elapsed = time.elapsed();
        times << elapsed;
        qDebug("Time elapsed: %d ms", elapsed);

        QVERIFY(fetchModel.rowCount() > 0);
    }

    summarizeResults(metaObject()->className(), times, logFile, startTime.secsTo(QDateTime::currentDateTime()));
}

void CallModelPerfTest::cleanupTestCase()
{
    if(logFile) {
        logFile->close();
        delete logFile;
        logFile = 0;
    }

    deleteAll();
}

QTEST_MAIN(CallModelPerfTest)
