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
#include "common.h"

using namespace CommHistory;

const int TIMEOUT = 5000;

void CallModelPerfTest::initTestCase()
{
    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    qsrand( QDateTime::currentDateTime().toTime_t() );
}

void CallModelPerfTest::init()
{
    deleteAll();
    QTest::qWait(TIMEOUT);
    waitForIdle();
}

void CallModelPerfTest::getEvents_data()
{
    QTest::addColumn<int>("events");
    QTest::addColumn<int>("contacts");

    QTest::newRow("10 events, 3 contacts") << 10 << 3;
    QTest::newRow("10 events, 300 contacts") << 10 << 300;
    QTest::newRow("100 events, 3 contacts") << 100 << 3;
    QTest::newRow("100 events, 300 contacts") << 100 << 300;
    QTest::newRow("1000 events, 3 contacts") << 1000 << 3;
    QTest::newRow("1000 events, 300 contacts") << 1000 << 300;
}

void CallModelPerfTest::getEvents()
{
    QFETCH(int, events);
    QFETCH(int, contacts);

    QDateTime startTime = QDateTime::currentDateTime();

    int commitBatchSize = 75;
    #ifdef PERF_BATCH_SIZE
    commitBatchSize = PERF_BATCH_SIZE;
    #endif


    EventModel addModel;
    addModel.enableContactChanges(false);
    QDateTime when = QDateTime::currentDateTime();
    QList<QString> remoteUids;

    qDebug() << __FUNCTION__ << "- Creating" << contacts << "new contacts";

    int ci = 0;
    while(ci < contacts) {
        ci++;
        QString phoneNumber = QString().setNum(qrand() % 10000000);
        remoteUids << phoneNumber;
        addTestContact(QString("Test Contact %1").arg(ci), phoneNumber);

        if(ci % commitBatchSize == 0 && ci < contacts) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "contacts (" << ci << "/" << contacts << ")";
            waitForIdle(5000);
        }
    }
    qDebug() << __FUNCTION__ << "- adding rest of the contacts ("
        << ci << "/" << contacts << ")";

    waitForIdle(5000);
    QTest::qWait(TIMEOUT);

    qDebug() << __FUNCTION__ << "- Creating" << events << "new events";

    QList<Event> eventList;

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
        e.setLocalUid(ACCOUNT1);
        e.setRemoteUid(remoteUids.at(qrand() % contacts));
        e.setFreeText("");
        e.setIsDraft(false);
        e.setIsMissedCall(isMissed);

        eventList << e;

        if(ei % commitBatchSize == 0 && ei != events) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "events (" << ei << "/" << events << ")";
            QVERIFY(addModel.addEvents(eventList, false));
            eventList.clear();
            waitForIdle();
        }
    }

    QVERIFY(addModel.addEvents(eventList, false));
    qDebug() << __FUNCTION__ << "- adding rest of the events ("
        << ei << "/" << events << ")";
    eventList.clear();
    waitForIdle();

    int iterations = 10;
    int sum = 0;
    QList<int> times;

    #ifdef PERF_ITERATIONS
    iterations = PERF_ITERATIONS;
    #endif

    char *iterVar = getenv("PERF_ITERATIONS");
    if (iterVar) {
        int iters = QString::fromAscii(iterVar).toInt();
        if (iters > 0) {
            iterations = iters;
        }
    }

    QTest::qWait(TIMEOUT);

    qDebug() << __FUNCTION__ << "- Fetching events." << iterations << "iterations";
    for(int i = 0; i < iterations; i++) {

        CallModel fetchModel;
        fetchModel.enableContactChanges(false);
        bool result = false;

        fetchModel.setQueryMode(EventModel::SyncQuery);
        fetchModel.setFilter(CallModel::SortByContact);

        QTime time;
        time.start();
        result = fetchModel.getEvents();
        int elapsed = time.elapsed();
        times << elapsed;
        sum += elapsed;
        qDebug("Time elapsed: %d ms", elapsed);

        QVERIFY(result);
        QVERIFY(fetchModel.rowCount() > 0);

        waitForIdle();
    }

    if(logFile) {
        QTextStream out(logFile);

        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << ": "
            << metaObject()->className() << "::" << QTest::currentTestFunction() << "("
            << QTest::currentDataTag() << ", " << iterations << " iterations)"
            << "\n";

        for (int i = 0; i < times.size(); i++) {
            out << times.at(i) << " ";
        }
        out << "\n";
    }

    qSort(times);
    float median = 0.0;
    if(iterations % 2 > 0) {
        median = times[(int)(iterations / 2)];
    } else {
        median = (times[iterations / 2] + times[iterations / 2 - 1]) / 2.0f;
    }

    float mean = sum / (float)iterations;
    int testSecs = startTime.secsTo(QDateTime::currentDateTime());

    qDebug("##### Mean: %.1f; Median: %.1f; Test time: %dsec", mean, median, testSecs);

    if(logFile) {
        QTextStream out(logFile);
        out << "Median average: " << (int)median << " ms. Test time: ";
        if (testSecs > 3600) { out << (testSecs / 3600) << "h "; }
        if (testSecs > 60) { out << ((testSecs % 3600) / 60) << "m "; }
        out << ((testSecs % 3600) % 60) << "s\n";
    }
}

void CallModelPerfTest::cleanupTestCase()
{
    deleteAll();
    QTest::qWait(TIMEOUT);
    waitForIdle();

    if(logFile) {
        logFile->close();
        delete logFile;
        logFile = 0;
    }
}

QTEST_MAIN(CallModelPerfTest)
