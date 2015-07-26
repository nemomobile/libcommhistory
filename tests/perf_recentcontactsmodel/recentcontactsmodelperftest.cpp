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
#include "recentcontactsmodelperftest.h"
#include "recentcontactsmodel.h"
#include "groupmodel.h"
#include "common.h"

using namespace CommHistory;

void RecentContactsModelPerfTest::initTestCase()
{
    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    qsrand( QDateTime::currentDateTime().toTime_t() );

    deleteAll();
}

void RecentContactsModelPerfTest::init()
{
}

void RecentContactsModelPerfTest::getEvents_data()
{
    QTest::addColumn<int>("events");
    QTest::addColumn<int>("contacts");
    QTest::addColumn<int>("limit");

    QTest::newRow("10 events, 10 contacts") << 10 << 10 << 0;
    QTest::newRow("10 events, 10 contacts with limit") << 10 << 10 << 3;
    QTest::newRow("100 events, 10 contacts") << 100 << 10 << 0;
    QTest::newRow("100 events, 10 contacts with limit") << 100 << 10 << 3;
    QTest::newRow("100 events, 100 contacts") << 100 << 100 << 0;
    QTest::newRow("100 events, 100 contacts with limit") << 100 << 100 << 33;
    QTest::newRow("1000 events, 100 contacts") << 1000 << 100 << 0;
    QTest::newRow("1000 events, 100 contacts with limit") << 1000 << 100 << 33;
    QTest::newRow("1000 events, 1000 contacts") << 1000 << 1000 << 0;
    QTest::newRow("1000 events, 1000 contacts with limit") << 1000 << 1000 << 333;
    QTest::newRow("10000 events, 1000 contacts") << 10000 << 1000 << 0;
    QTest::newRow("10000 events, 1000 contacts with limit") << 10000 << 1000 << 333;
}

void RecentContactsModelPerfTest::getEvents()
{
    QFETCH(int, events);
    QFETCH(int, contacts);
    QFETCH(int, limit);
    const int groups = contacts/2;

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

        contactDetails.append(qMakePair(QString("Test Contact %1").arg(ci), qMakePair(phoneNumber, (ci <= groups ? ACCOUNT1 : QString()))));

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

    qDebug() << __FUNCTION__ << "- Creating" << groups << "new groups";

    GroupModel groupModel;
    QList<Group> groupList;

    int gi = 0;
    while(gi < groups) {
        Group grp;
        grp.setLocalUid(ACCOUNT1);
        grp.setRecipients(RecipientList::fromUids(ACCOUNT1, QStringList() << remoteUids.at(contactIndices.at(gi))));

        QVERIFY(groupModel.addGroup(grp));
        groupList << grp;

        gi++;
        if(gi % commitBatchSize == 0 && gi < groups) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "groups (" << gi << "/" << groups << ")";
        }
    }
    qDebug() << __FUNCTION__ << "- adding rest of the groups ("
             << gi << "/" << groups << ")";

    qDebug() << __FUNCTION__ << "- Creating" << events << "new events";

    QList<Event> eventList;

    int ei = 0;
    while(ei < events) {
        ei++;

        int idIndex = contactIndices.at(qrand() % contacts);
        bool isCall(idIndex >= groups);
        Event::EventDirection direction(qrand() % 2 ? Event::Outbound : Event::Inbound);
        bool isMissed(isCall && qrand() % 2);

        Event e;
        e.setType(isCall ? Event::CallEvent : Event::IMEvent);
        e.setGroupId(isCall ? -1 : groupList.at(idIndex).id());
        e.setDirection(direction);
        e.setStartTime(when.addSecs(ei));
        e.setEndTime(when.addSecs(ei));
        e.setLocalUid(isCall ? RING_ACCOUNT : ACCOUNT1);
        e.setRecipients(Recipient(e.localUid(), remoteUids.at(idIndex)));
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

        RecentContactsModel fetchModel;
        if (limit)
            fetchModel.setLimit(limit);

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

void RecentContactsModelPerfTest::cleanupTestCase()
{
    if(logFile) {
        logFile->close();
        delete logFile;
        logFile = 0;
    }

    deleteAll();
}

QTEST_MAIN(RecentContactsModelPerfTest)
