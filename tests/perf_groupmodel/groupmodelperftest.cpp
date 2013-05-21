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
#include <QModelIndex>
#include <cstdlib>
#include "groupmodelperftest.h"
#include "common.h"

using namespace CommHistory;

const int TIMEOUT = 5000;

void GroupModelPerfTest::initTestCase()
{
    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    qsrand( QDateTime::currentDateTime().toTime_t() );
}

void GroupModelPerfTest::init()
{
    deleteAll();
    QTest::qWait(TIMEOUT);
    waitForIdle();
}

void GroupModelPerfTest::getGroups_data()
{
    QTest::addColumn<int>("groups");
    QTest::addColumn<int>("messages");

    QTest::newRow("10 groups, 1 message each") << 10 << 1;
    QTest::newRow("10 groups, 10 messages each") << 10 << 10;
    QTest::newRow("100 groups, 1 message each") << 100 << 1;
    QTest::newRow("100 groups, 10 messages each") << 100 << 10;
}

void GroupModelPerfTest::getGroups()
{
    qDebug() << __FUNCTION__;

    QDateTime startTime = QDateTime::currentDateTime();

    QFETCH(int, groups);
    QFETCH(int, messages);

    int commitBatchSize = 75;
    #ifdef PERF_BATCH_SIZE
    commitBatchSize = PERF_BATCH_SIZE;
    #endif

    GroupModel addModel;
    QDateTime when = QDateTime::currentDateTime();
    QList<Group> groupList;

    qDebug() << __FUNCTION__ << "- Creating" << groups << "new contacts and groups";

    int gi = 0;
    while(gi < groups) {
        gi++;

        QString phoneNumber = QString().setNum(qrand() % 10000000);
        addTestContact(QString("Test Contact %1").arg(gi), phoneNumber);

        Group grp;
        grp.setLocalUid(ACCOUNT1);
        QStringList uids;
        uids << phoneNumber;
        grp.setRemoteUids(uids);

        QVERIFY(addModel.addGroup(grp));
        groupList << grp;

        if(gi % commitBatchSize == 0 && gi < groups) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "groups (" << gi << "/" << groups << ")";
            waitForIdle(5000);
        }
    }
    qDebug() << __FUNCTION__ << "- adding rest of the groups ("
             << gi << "/" << groups << ")";
    waitForIdle(5000);
    QTest::qWait(TIMEOUT);

    qDebug() << __FUNCTION__ << "- Creating" << messages << "messages to each group";

    EventModel eventModel;

    gi = 0;
    foreach (Group grp, groupList) {
        gi++;
        QList<Event> eventList;

        for(int i = 0; i < messages; i++) {

            Event e;
            e.setType(Event::SMSEvent);
            e.setDirection(qrand() % 2 ? Event::Inbound : Event::Outbound);
            e.setGroupId(grp.id());
            e.setStartTime(when.addSecs(i));
            e.setEndTime(when.addSecs(i));
            e.setLocalUid(ACCOUNT1);
            e.setRemoteUid(grp.remoteUids().at(0));
            e.setFreeText(randomMessage(qrand() % 49 + 1));  // Max 50 words / message
            e.setIsDraft(false);
            e.setIsMissedCall(false);

            eventList << e;
        }

        QVERIFY(eventModel.addEvents(eventList, false));
        qDebug() << __FUNCTION__ << "- adding" << messages << "messages ("
                 << gi << "/" << groups << ")";
        eventList.clear();
        waitForIdle();
    }

    int iterations = 10;
    int sum = 0;
    QList<int> times;

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

    QTest::qWait(TIMEOUT);

    qDebug() << __FUNCTION__ << "- Fetching groups." << iterations << "iterations";
    for(int i = 0; i < iterations; i++) {

        GroupModel fetchModel;
        bool result = false;

        fetchModel.setQueryMode(EventModel::SyncQuery);

        QTime time;
        time.start();
        result = fetchModel.getGroups();
        int elapsed = time.elapsed();
        times << elapsed;
        sum += elapsed;
        qDebug("Time elapsed: %d ms", elapsed);

        QVERIFY(result);
        QCOMPARE(fetchModel.rowCount(), groups);
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

void GroupModelPerfTest::cleanupTestCase()
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

QTEST_MAIN(GroupModelPerfTest)
