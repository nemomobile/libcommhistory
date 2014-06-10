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
#include "conversationmodelperftest.h"
#include "conversationmodel.h"
#include "groupmodel.h"
#include "common.h"

using namespace CommHistory;

void ConversationModelPerfTest::initTestCase()
{
    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    qsrand( QDateTime::currentDateTime().toTime_t() );

    deleteAll();
}

void ConversationModelPerfTest::init()
{
}

void ConversationModelPerfTest::getEvents_data()
{
    // Number of messages created
    QTest::addColumn<int>("messages");

    // Number of contacts created
    QTest::addColumn<int>("contacts");

    // Number of messages to fetch from db. Negative value fetches all messages
    QTest::addColumn<int>("limit");

    // Whether to resolve UIDs to contacts
    QTest::addColumn<bool>("resolve");

    QTest::newRow("10 messages, 3 contacts") << 10 << 3 << -1 << false;
    QTest::newRow("10 messages, 3 contacts with resolve") << 10 << 3 << -1 << true;
    QTest::newRow("100 messages, 3 contacts") << 100 << 3 << -1 << false;
    QTest::newRow("100 messages, 3 contacts with resolve") << 100 << 3 << -1 << true;
    QTest::newRow("1000 messages, 3 contacts") << 1000 << 3 << -1 << false;
    QTest::newRow("1000 messages, 3 contacts with resolve") << 1000 << 3 << -1 << true;
    QTest::newRow("1000 messages, 3 contacts, limit 25") << 1000 << 3 << 25 << false;
    QTest::newRow("1000 messages, 3 contacts, limit 25 with resolve") << 1000 << 3 << 25 << true;
    QTest::newRow("10 messages, 300 contacts") << 10 << 300 << -1 << false;
    QTest::newRow("10 messages, 300 contacts with resolve") << 10 << 300 << -1 << true;
    QTest::newRow("100 messages, 300 contacts") << 100 << 300 << -1 << false;
    QTest::newRow("100 messages, 300 contacts with resolve") << 100 << 300 << -1 << true;
    QTest::newRow("1000 messages, 300 contacts") << 1000 << 300 << -1 << false;
    QTest::newRow("1000 messages, 300 contacts with resolve") << 1000 << 300 << -1 << true;
    QTest::newRow("1000 messages, 300 contacts, limit 25") << 1000 << 300 << 25 << false;
    QTest::newRow("1000 messages, 300 contacts, limit 25 with resolve") << 1000 << 300 << 25 << true;
}

void ConversationModelPerfTest::getEvents()
{
    QFETCH(int, messages);
    QFETCH(int, contacts);
    QFETCH(int, limit);
    QFETCH(bool, resolve);

    qRegisterMetaType<QModelIndex>("QModelIndex");

    QDateTime startTime = QDateTime::currentDateTime();

    cleanupTestGroups();
    cleanupTestEvents();

    int commitBatchSize = 75;
    #ifdef PERF_BATCH_SIZE
    commitBatchSize = PERF_BATCH_SIZE;
    #endif

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

    // Randomize the contact indices
    random_shuffle(contactIndices.begin(), contactIndices.end());

    qDebug() << __FUNCTION__ << "- Creating" << contacts << "new groups";

    QList<Group> groupList;
    QList<int> groupIds;

    GroupModel groupModel;

    int gi = 0;
    while(gi < contacts) {
        Group grp;
        grp.setLocalUid(RING_ACCOUNT);
        grp.setRecipients(RecipientList::fromUids(RING_ACCOUNT, QStringList() << remoteUids.at(contactIndices.at(gi))));

        QVERIFY(groupModel.addGroup(grp));
        groupList << grp;
        groupIds << grp.id();

        gi++;
        if(gi % commitBatchSize == 0 && gi < contacts) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "groups (" << gi << "/" << contacts << ")";
        }
    }
    qDebug() << __FUNCTION__ << "- adding rest of the groups ("
             << gi << "/" << contacts << ")";

    EventModel addModel;
    QDateTime when = QDateTime::currentDateTime();

    qDebug() << __FUNCTION__ << "- Creating" << messages << "new messages";

    QList<Event> eventList;

    int ei = 0;
    while(ei < messages) {
        ei++;

        Event::EventDirection direction;
        direction = qrand() % 2 > 0 ? Event::Inbound : Event::Outbound;

        const int index(contactIndices.at(qrand() % contacts));

        Event e;
        e.setType(Event::SMSEvent);
        e.setDirection(direction);
        e.setGroupId(groupList.at(index).id());
        e.setStartTime(when.addSecs(ei));
        e.setEndTime(when.addSecs(ei));
        e.setLocalUid(RING_ACCOUNT);
        e.setRecipients(Recipient(RING_ACCOUNT, remoteUids.at(index)));
        e.setFreeText(randomMessage(qrand() % 49 + 1)); // Max 50 words / message
        e.setIsDraft(false);
        e.setIsMissedCall(false);

        eventList << e;

        if(ei % commitBatchSize == 0 && ei != messages) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "messages (" << ei << "/" << messages << ")";
            QVERIFY(addModel.addEvents(eventList, false));
            eventList.clear();
        }
    }

    QVERIFY(addModel.addEvents(eventList, false));
    qDebug() << __FUNCTION__ << "- adding rest of the messages ("
        << ei << "/" << messages << ")";
    eventList.clear();

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

    qDebug() << __FUNCTION__ << "- Fetching messages." << iterations << "iterations";
    for(int i = 0; i < iterations; i++) {

        ConversationModel fetchModel;
        fetchModel.setResolveContacts(resolve);

        if (limit > 0) {
            fetchModel.setQueryMode(EventModel::StreamedAsyncQuery);
            fetchModel.setFirstChunkSize(limit);
            fetchModel.setChunkSize(limit);
        }

        waitForIdle();

        QTime time;
        time.start();
        bool result = fetchModel.getEvents(groupIds);
        QVERIFY(result);

        if (!fetchModel.isReady())
            waitForSignal(&fetchModel, SIGNAL(modelReady(bool)));

        int elapsed = time.elapsed();
        times << elapsed;
        sum += elapsed;
        qDebug("Time elapsed: %d ms", elapsed);

        QVERIFY(fetchModel.rowCount() > 0);
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

    qDebug("##### Mean: %.1f; Median: %.1f; Min: %d; Max: %d; Test time: %dsec", mean, median, times[0], times[iterations-1], testSecs);

    if(logFile) {
        QTextStream out(logFile);
        out << "Median average: " << (int)median << " ms. Min:" << times[0] << "ms. Max:" << times[iterations-1] << " ms. Test time: ";
        if (testSecs > 3600) { out << (testSecs / 3600) << "h "; }
        if (testSecs > 60) { out << ((testSecs % 3600) / 60) << "m "; }
        out << ((testSecs % 3600) % 60) << "s\n";
    }
}

void ConversationModelPerfTest::cleanupTestCase()
{
    if(logFile) {
        logFile->close();
        delete logFile;
        logFile = 0;
    }

    deleteAll();
}

QTEST_MAIN(ConversationModelPerfTest)
