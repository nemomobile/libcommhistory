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

#include "groupmodelprofiletest.h"
#include "groupmodel.h"
#include "common.h"
#include <QtTest/QtTest>
#include <QDateTime>
#include <QDBusConnection>
#include <QModelIndex>
#include <cstdlib>

using namespace CommHistory;

void GroupModelProfileTest::initTestCase()
{
    logFile = 0;

    qsrand( QDateTime::currentDateTime().toTime_t() );
}

void GroupModelProfileTest::init()
{
}

void GroupModelProfileTest::prepare()
{
    const int groups = 1000;
    const int messages = 10;
    const int contacts = 1000;

    deleteAll();

    int commitBatchSize = 100;

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

    qDebug() << __FUNCTION__ << "- Creating" << groups << "new groups";

    QList<Group> groupList;
    GroupModel addModel;

    int gi = 0;
    while(gi < groups) {
        Group grp;
        grp.setLocalUid(RING_ACCOUNT);
        grp.setRecipients(RecipientList::fromUids(RING_ACCOUNT, QStringList() << remoteUids.at(contactIndices.at(gi))));

        QVERIFY(addModel.addGroup(grp));
        groupList << grp;

        gi++;
        if(gi % commitBatchSize == 0 && gi < groups) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "groups (" << gi << "/" << groups << ")";
        }
    }
    qDebug() << __FUNCTION__ << "- adding rest of the groups ("
             << gi << "/" << groups << ")";

    qDebug() << __FUNCTION__ << "- Creating" << messages << "messages to each group";

    EventModel eventModel;
    QDateTime when = QDateTime::currentDateTime();

    gi = 0;
    foreach (Group grp, groupList) {
        QList<Event> eventList;
        for(int i = 0; i < messages; i++) {

            Event e;
            e.setType(Event::SMSEvent);
            e.setDirection(qrand() % 2 ? Event::Inbound : Event::Outbound);
            e.setGroupId(grp.id());
            e.setStartTime(when.addSecs(i));
            e.setEndTime(when.addSecs(i));
            e.setLocalUid(RING_ACCOUNT);
            e.setRecipients(grp.recipients());
            e.setFreeText(randomMessage(qrand() % 49 + 1));  // Max 50 words / message
            e.setIsDraft(false);
            e.setIsMissedCall(false);

            eventList << e;
        }

        QVERIFY(eventModel.addEvents(eventList, false));
        gi++;
        qDebug() << __FUNCTION__ << "- adding" << messages << "messages ("
                 << gi << "/" << groups << ")";
        eventList.clear();
    }
}

void GroupModelProfileTest::execute()
{
    const bool resolve = false;

    QList<int> times;

    int iterations = 1;

    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    QDateTime startTime = QDateTime::currentDateTime();

    qDebug() << __FUNCTION__ << "- Fetching groups." << iterations << "iterations";
    for(int i = 0; i < iterations; i++) {

        GroupModel fetchModel;

        GroupManager manager;
        manager.setResolveContacts(resolve ? GroupManager::ResolveImmediately : GroupManager::DoNotResolve);

        fetchModel.setManager(&manager);

        waitForIdle();

        QTime time;
        time.start();

        bool result = fetchModel.getGroups();
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

void GroupModelProfileTest::finalise()
{
    deleteAll();
}

void GroupModelProfileTest::cleanupTestCase()
{
    if(logFile) {
        logFile->close();
        delete logFile;
        logFile = 0;
    }
}

QTEST_MAIN(GroupModelProfileTest)
