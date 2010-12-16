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
#include <QDateTime>
#include <QDBusConnection>
#include <cstdlib>
#include "conversationmodelperftest.h"
#include "common.h"
#include "conversationmodel.h"

using namespace CommHistory;

Group group1, group2;

const int TIMEOUT = 5000;

void ConversationModelPerfTest::initTestCase()
{
    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    qsrand( QDateTime::currentDateTime().toTime_t() );
}

void ConversationModelPerfTest::init()
{
    deleteAll();
    QTest::qWait(TIMEOUT);
    waitForIdle();
}

void ConversationModelPerfTest::getEvents_data()
{
    // Number of messages created
    QTest::addColumn<int>("messages");

    // Number of contacts created
    QTest::addColumn<int>("contacts");

    // Number of messages to fetch from db. Negative value fetches all messages
    QTest::addColumn<int>("limit");

    QTest::newRow("10 messages, 3 contacts") << 10 << 3 << -1;
    QTest::newRow("10 messages, 300 contacts") << 10 << 300 << -1;
    QTest::newRow("100 messages, 3 contacts") << 100 << 3 << -1;
    QTest::newRow("100 messages, 300 contacts") << 100 << 300 << -1;
    QTest::newRow("1000 messages, 3 contacts") << 1000 << 3 << -1;
    QTest::newRow("1000 messages, 300 contacts") << 1000 << 300 << -1;
    QTest::newRow("1000 messages, 3 contacts, limit 25") << 1000 << 3 << 25;
    QTest::newRow("1000 messages, 300 contacts, limit 25") << 1000 << 300 << 25;
}

void ConversationModelPerfTest::getEvents()
{
    QFETCH(int, messages);
    QFETCH(int, contacts);
    QFETCH(int, limit);

    QDateTime startTime = QDateTime::currentDateTime();

    addTestGroups( group1, group2 );

    int commitBatchSize = 75;
    #ifdef PERF_BATCH_SIZE
    commitBatchSize = PERF_BATCH_SIZE;
    #endif

    EventModel addModel;
    QDateTime when = QDateTime::currentDateTime();
    QList<QString> remoteUids;

    qDebug() << __FUNCTION__ << "- Creating" << contacts << "new contacts";

    SopranoLive::RDFTransactionPtr contactTransaction;
    contactTransaction = ::tracker()->createTransaction();

    int ci = 0;
    while(ci < contacts) {
        ci++;
        QString phoneNumber = QString().setNum(qrand() % 10000000);
        remoteUids << phoneNumber;
        addTestContact(QString("Test Contact %1").arg(ci), phoneNumber);

        if(ci % commitBatchSize == 0 && ci < contacts) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "contacts (" << ci << "/" << contacts << ")";
            contactTransaction->commitAndReinitiate(true);
            waitForIdle(5000);
        }
    }
    qDebug() << __FUNCTION__ << "- adding rest of the contacts ("
        << ci << "/" << contacts << ")";
    contactTransaction->commit(true);
    waitForIdle(5000);
    QTest::qWait(TIMEOUT);

    qDebug() << __FUNCTION__ << "- Creating" << messages << "new messages";

    QList<Event> eventList;
    SopranoLive::RDFTransactionPtr eventTransaction;
    eventTransaction = ::tracker()->createTransaction();

    int ei = 0;
    while(ei < messages) {
        ei++;

        Event::EventDirection direction;
        direction = qrand() % 2 > 0 ? Event::Inbound : Event::Outbound;

        Event e;
        e.setType(Event::SMSEvent);
        e.setDirection(direction);
        e.setGroupId(group1.id());
        e.setStartTime(when.addSecs(ei));
        e.setEndTime(when.addSecs(ei));
        e.setLocalUid(ACCOUNT1);
        e.setRemoteUid(remoteUids.at(0));
        e.setFreeText(randomMessage(qrand() % 49 + 1)); // Max 50 words / message
        e.setIsDraft(false);
        e.setIsMissedCall(false);

        eventList << e;

        if(ei % commitBatchSize == 0 && ei != messages) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "messages (" << ei << "/" << messages << ")";
            QVERIFY(addModel.addEvents(eventList, false));
            eventTransaction->commitAndReinitiate(true);
            eventList.clear();
            waitForIdle();
        }
    }

    QVERIFY(addModel.addEvents(eventList, false));
    qDebug() << __FUNCTION__ << "- adding rest of the messages ("
        << ei << "/" << messages << ")";
    eventTransaction->commit(true);
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

    qDebug() << __FUNCTION__ << "- Fetching messages." << iterations << "iterations";
    for(int i = 0; i < iterations; i++) {

        ConversationModel fetchModel;
        bool result = false;

        QSignalSpy rowsInserted(&fetchModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)));

        if (limit < 0) {
            fetchModel.setQueryMode(EventModel::SyncQuery);
        } else {
            fetchModel.setQueryMode(EventModel::StreamedAsyncQuery);
            fetchModel.setChunkSize(limit);
        }

        QTime time;
        time.start();
        result = fetchModel.getEvents(group1.id());

        if(limit >= 0) {
            while (time.elapsed() < 10000 && rowsInserted.isEmpty())
            QCoreApplication::processEvents();
        }

        int elapsed = time.elapsed();
        times << elapsed;
        sum += elapsed;
        qDebug("Time elapsed: %d ms", elapsed);

        QVERIFY(result);
        QVERIFY(fetchModel.rowCount() > 0);

        // With 1000 messages deleting model right away results in segfault
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

void ConversationModelPerfTest::cleanupTestCase()
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

QTEST_MAIN(ConversationModelPerfTest)
