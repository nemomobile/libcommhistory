/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
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
#include "recentcontactsmodelprofiletest.h"
#include "recentcontactsmodel.h"
#include "groupmodel.h"
#include "common.h"

using namespace CommHistory;

void RecentContactsModelProfileTest::initTestCase()
{
    logFile = 0;

    qsrand( QDateTime::currentDateTime().toTime_t() );
}

void RecentContactsModelProfileTest::init()
{
}

void RecentContactsModelProfileTest::prepare()
{
    const int events = 10000;
    const int contacts = 1000;
    const int groups = contacts/2;

    deleteAll();

    int commitBatchSize = 100;

    EventModel addModel;
    QDateTime when = QDateTime::currentDateTime();

    qDebug() << Q_FUNC_INFO << "- Creating" << contacts << "contacts";

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
            qDebug() << Q_FUNC_INFO << "- adding" << commitBatchSize
                << "contacts (" << ci << "/" << contacts << ")";
            addTestContacts(contactDetails);
            contactDetails.clear();
        }
    }
    if (!contactDetails.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "- adding rest of the contacts ("
            << ci << "/" << contacts << ")";
        addTestContacts(contactDetails);
        contactDetails.clear();
    }

    qDebug() << Q_FUNC_INFO << "- Creating" << groups << "new groups";

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
            qDebug() << Q_FUNC_INFO << "- adding" << commitBatchSize
                << "groups (" << gi << "/" << groups << ")";
        }
    }
    qDebug() << Q_FUNC_INFO << "- adding rest of the groups ("
             << gi << "/" << groups << ")";

    qDebug() << Q_FUNC_INFO << "- Creating" << events << "new events";

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
            qDebug() << Q_FUNC_INFO << "- adding" << commitBatchSize
                << "events (" << ei << "/" << events << ")";
            QVERIFY(addModel.addEvents(eventList, false));
            eventList.clear();
        }
    }

    QVERIFY(addModel.addEvents(eventList, false));
    qDebug() << Q_FUNC_INFO << "- adding rest of the events ("
        << ei << "/" << events << ")";
    eventList.clear();
}

void RecentContactsModelProfileTest::execute()
{
    const int limit = 333;

    QList<int> times;

    int iterations = 1;

    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    QDateTime startTime = QDateTime::currentDateTime();

    qDebug() << Q_FUNC_INFO << "- Fetching events." << iterations << "iterations";
    for(int i = 0; i < iterations; i++) {

        RecentContactsModel fetchModel;
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

void RecentContactsModelProfileTest::finalise()
{
    deleteAll();
}

void RecentContactsModelProfileTest::cleanupTestCase()
{
    if(logFile) {
        logFile->close();
        delete logFile;
        logFile = 0;
    }
}

QTEST_MAIN(RecentContactsModelProfileTest)
