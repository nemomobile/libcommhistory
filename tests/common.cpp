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

#include <QSparqlConnection>
#include <QSparqlResult>
#include <QSparqlQuery>
#include <QSparqlError>

#include <QFile>
#include <QTextStream>

#include "eventmodel.h"
#include "groupmodel.h"
#include "event.h"
#include "common.h"
#include "trackerio.h"
#include "commonutils.h"

namespace {
static int contactNumber = 0;
};

using namespace CommHistory;

const int numWords = 23;
const char* msgWords[] = { "lorem","ipsum","dolor","sit","amet","consectetur",
    "adipiscing","elit","in","imperdiet","cursus","lacus","vitae","suscipit",
    "maecenas","bibendum","rutrum","dolor","at","hendrerit",":)",":P","OMG!!" };
int ticks = 0;
int idleTicks = 0;

int addTestEvent(EventModel &model,
                 Event::EventType type,
                 Event::EventDirection direction,
                 const QString &account,
                 int groupId,
                 const QString &text,
                 bool isDraft,
                 bool isMissedCall,
                 const QDateTime &when,
                 const QString &remoteUid,
                 bool toModelOnly,
                 const QString messageToken)
{
    Event event;
    event.setType(type);
    event.setDirection(direction);
    event.setGroupId(groupId);
    event.setStartTime(when);
    event.setEndTime(when);
    event.setLocalUid(account);
    if (remoteUid.isEmpty()) {
        event.setRemoteUid(type == Event::SMSEvent ? "555123456" : "td@localhost");
    } else {
        event.setRemoteUid(remoteUid);
    }
    event.setFreeText(text);
    event.setIsDraft( isDraft );
    event.setIsMissedCall( isMissedCall );
    event.setMessageToken(messageToken);
    if (model.addEvent(event, toModelOnly)) {
        return event.id();
    }
    return -1;
}

void addTestGroups(Group &group1, Group &group2)
{
    addTestGroup(group1,
                 "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0",
                 "td@localhost");
    addTestGroup(group2,
                 "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0",
                 "td2@localhost");
}

void addTestGroup(Group& grp, QString localUid, QString remoteUid)
{
    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    grp.setLocalUid(localUid);
    QStringList uids;
    uids << remoteUid;
    grp.setRemoteUids(uids);
    QSignalSpy ready(&groupModel, SIGNAL(groupsCommitted(QList<int>,bool)));
    QVERIFY(groupModel.addGroup(grp));

    QVERIFY(waitSignal(ready, 1000));
    QVERIFY(ready.first().at(1).toBool());
}

int addTestContact(const QString &name, const QString &remoteUid, const QString &localUid)
{
    QString contactUri = QString("<testcontact:%1>").arg(contactNumber++);
    QString addContact("INSERT { "
                       " %1 "
                       " %2 a nco:PersonContact ; "
                       " nco:hasAffiliation _:foo ; "
                       " nco:nameFamily \"%3\" . }");

    QString addAffiliation("_:foo a nco:Affiliation; ");

    QString addressQuery;
    QString normal = CommHistory::normalizePhoneNumber(remoteUid);
    if (normal.isEmpty()) {
        QString uri = QString("telepathy:%1!%2").arg(localUid).arg(remoteUid);
        addressQuery = QString("INSERT { <%1> a nco:IMAddress }").arg(uri);
        addAffiliation += QString("nco:hasIMAddress <%1> .").arg(uri);
    } else {
        QString shortNumber = makeShortNumber(remoteUid);
        addressQuery =
            QString("INSERT { _:_ a nco:PhoneNumber; nco:phoneNumber \"%1\"; "
                    "maemo:localPhoneNumber \"%2\" . } "
                    "WHERE { "
                    "OPTIONAL { ?tel nco:phoneNumber \"%1\" } "
                    "FILTER(!BOUND(?tel)) "
                    "}")
            .arg(remoteUid)
            .arg(shortNumber);
        addAffiliation += QString("nco:hasPhoneNumber ?tel .");
        addContact += QString(" WHERE { ?tel a nco:PhoneNumber; nco:phoneNumber \"%1\" }")
            .arg(remoteUid);
    }

    QString query = addressQuery + " " + QString(addContact).arg(addAffiliation).arg(contactUri).arg(name);
    QSparqlQuery insertQuery(query, QSparqlQuery::InsertStatement);
    QScopedPointer<QSparqlConnection> conn(new QSparqlConnection(QLatin1String("QTRACKER")));
    QScopedPointer<QSparqlResult> result(conn->exec(insertQuery));
    result->waitForFinished();
    if (result->hasError()) {
        qWarning() << "error inserting address:" << result->lastError().message();
        return -1;
    }

    query = QString("SELECT tracker:id(?c) WHERE { ?c a nco:PersonContact. FILTER(?c = %1) }")
        .arg(contactUri);
    result.reset(conn->exec(QSparqlQuery(query)));
    result->waitForFinished();
    if (result->hasError() || !result->first()) {
        qWarning() << "error getting id of inserted contact:" << result->lastError().message();
        return -1;
    }

    qDebug() << "********** contact id" << result->value(0).toInt();

    return result->value(0).toInt();
}

void modifyTestContact(int id, const QString &name)
{
    qDebug() << Q_FUNC_INFO << id << name;

    QString query("DELETE { ?contact nco:nameFamily ?name } WHERE "
                  "{ ?contact a nco:PersonContact; nco:nameFamily ?name . "
                  "FILTER(tracker:id(?contact) = %1) }");
    QScopedPointer<QSparqlConnection> conn(new QSparqlConnection(QLatin1String("QTRACKER")));
    QScopedPointer<QSparqlResult> result(conn->exec(QSparqlQuery(query.arg(QString::number(id)),
                                                                 QSparqlQuery::DeleteStatement)));
    result->waitForFinished();
    if (result->hasError()) {
        qWarning() << "error modifying contact:" << result->lastError().message();
        return;
    }

    QString addContact("INSERT { ?c nco:nameFamily \"%2\" } "
                       "WHERE {?c a nco:PersonContact . FILTER(tracker:id(?c) = %1) }");
    QScopedPointer<QSparqlResult> result2(conn->exec(QSparqlQuery(addContact.arg(id).arg(name),
                                                                  QSparqlQuery::InsertStatement)));

    result2->waitForFinished();
    if (result2->hasError()) {
        qWarning() << "error modifying contact:" << result2->lastError().message();
        return;
    }
}

void deleteTestContact(int id)
{
    QString query("DELETE { ?aff a nco:Affiliation } WHERE"
                  "{ ?c a nco:PersonContact; nco:hasAffiliation ?aff . "
                  "FILTER(tracker:id(?c) = %1) } "
                  "DELETE { ?contact a nco:PersonContact } WHERE"
                  "{ ?contact a nco:PersonContact . "
                  "FILTER(tracker:id(?contact) = %1) }");
    QScopedPointer<QSparqlConnection> conn(new QSparqlConnection(QLatin1String("QTRACKER")));
    QScopedPointer<QSparqlResult> result(conn->exec(QSparqlQuery(query.arg(QString::number(id)),
                                                                 QSparqlQuery::DeleteStatement)));
    result->waitForFinished();
    if (result->hasError()) {
        qWarning() << "error deleting contact:" << result->lastError().message();
        return;
    }
}

bool compareEvents(Event &e1, Event &e2)
{
    if (e1.type() != e2.type()) {
        qWarning() << Q_FUNC_INFO << "type:" << e1.type() << e2.type();
        return false;
    }
    if (e1.direction() != e2.direction()) {
        qWarning() << Q_FUNC_INFO << "direction:" << e1.direction() << e2.direction();
        return false;
    }
    if (e1.startTime().toTime_t() != e2.startTime().toTime_t()) {
        qWarning() << Q_FUNC_INFO << "startTime:" << e1.startTime() << e2.startTime();
        return false;
    }
    if (e1.endTime().toTime_t() != e2.endTime().toTime_t()) {
        qWarning() << Q_FUNC_INFO << "endTime:" << e1.endTime() << e2.endTime();
        return false;
    }
    if (e1.isDraft() != e2.isDraft()) {
        qWarning() << Q_FUNC_INFO << "isDraft:" << e1.isDraft() << e2.isDraft();
        return false;
    }
    if (e1.isRead() != e2.isRead()) {
        qWarning() << Q_FUNC_INFO << "isRead:" << e1.isRead() << e2.isRead();
        return false;
    }
    if (e1.type() == Event::CallEvent && e1.isMissedCall() != e2.isMissedCall()) {
        qWarning() << Q_FUNC_INFO << "isMissedCall:" << e1.isMissedCall() << e2.isMissedCall();
        return false;
    }
    if (e1.type() == Event::CallEvent && e1.isEmergencyCall() != e2.isEmergencyCall()) {
        qWarning() << Q_FUNC_INFO << "isEmergencyCall:" << e1.isEmergencyCall() << e2.isEmergencyCall();
        return false;
    }
//    QCOMPARE(e1.bytesSent(), e2.bytesSent());
//    QCOMPARE(e1.bytesReceived(), e2.bytesReceived());
    if (e1.localUid() != e2.localUid()) {
        qWarning() << Q_FUNC_INFO << "localUid:" << e1.localUid() << e2.localUid();
        return false;
    }
    if (e1.remoteUid() != e2.remoteUid()) {
        qWarning() << Q_FUNC_INFO << "remoteUid:" << e1.remoteUid() << e2.remoteUid();
        return false;
    }
    if (e1.type() != Event::CallEvent && e1.freeText() != e2.freeText()) {
        qWarning() << Q_FUNC_INFO << "freeText:" << e1.freeText() << e2.freeText();
        return false;
    }
    if (e1.groupId() != e2.groupId()) {
        qWarning() << Q_FUNC_INFO << "groupId:" << e1.groupId() << e2.groupId();
        return false;
    }
    if (e1.type() != Event::CallEvent && e1.fromVCardFileName() != e2.fromVCardFileName()) {
        qWarning() << Q_FUNC_INFO << "vcardFileName:" << e1.fromVCardFileName() << e2.fromVCardFileName();
        return false;
    }
    if (e1.type() != Event::CallEvent && e1.fromVCardLabel() != e2.fromVCardLabel()) {
        qWarning() << Q_FUNC_INFO << "vcardLabel:" << e1.fromVCardLabel() << e2.fromVCardLabel();
        return false;
    }
    if (e1.type() != Event::CallEvent && e1.status() != e2.status()) {
        qWarning() << Q_FUNC_INFO << "status:" << e1.status() << e2.status();
        return false;
    }

//    QCOMPARE(e1.messageToken(), e2.messageToken());
    return true;
}

void deleteAll()
{
    qDebug() << __FUNCTION__ << "- Deleting all";

    QScopedPointer<QSparqlConnection> conn(new QSparqlConnection(QLatin1String("QTRACKER_DIRECT")));
    QSparqlQuery query(QLatin1String(
            "DELETE {?n a rdfs:Resource}"
            "WHERE {?n rdf:type ?t FILTER(?t IN (nmo:Message,"
                                                "nmo:Attachment,"
                                                "nmo:Multipart,"
                                                "nmo:CommunicationChannel,"
                                                "nco:IMAddress,"
                                                "nco:PhoneNumber))}"),
                       QSparqlQuery::DeleteStatement);
    QSparqlResult* result = conn->exec(query);
    result->waitForFinished();
    if (result->hasError())
        qDebug() << result->lastError().message();
}

void deleteSmsMsgs()
{
    QScopedPointer<QSparqlConnection> conn(new QSparqlConnection(QLatin1String("QTRACKER_DIRECT")));
    QSparqlQuery query(QLatin1String("DELETE {?n a rdfs:Resource} WHERE {?n rdf:type nmo:SMSMessage}"),
                       QSparqlQuery::DeleteStatement);
    QSparqlResult* result = conn->exec(query);
    result->waitForFinished();
    if (result->hasError())
        qDebug() << result->lastError().message();
}

QString randomMessage(int words)
{
    QString msg;
    QTextStream msgStream(&msg, QIODevice::WriteOnly);
    for(int j = 0; j < words; j++) {
        msgStream << msgWords[qrand() % numWords] << " ";
    }
    return msg;
}

/*
 * Returns the average system load since last time this function was called (or
 * since boot if this first time this function is called). The scale is [1, 0],
 * where 1 is busy and 0 is idle system. In case of errors, -1 is returned.
 */
double getSystemLoad()
{
    // Parsing assumes that first line of /proc/stat looks something like this:
    // cpu  110807 325 23491 425840 37435 1367 32 0 0

    QFile file("/proc/stat");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << __PRETTY_FUNCTION__ << "Failed to open /proc/stat";
        return -1;
    }

    QTextStream in(&file);
    QString line = in.readLine();
    file.close();

    QStringList parts = line.split(" ", QString::SkipEmptyParts);
    if (parts.size() != 10) {
        qWarning() << __PRETTY_FUNCTION__ << "Invalid input from /proc/stat:" << line;
        return -1;
    }

    int newIdleTicks = parts.at(4).toInt();
    int newAllTicks = 0;
    for (int i = 1; i < 10; i++) {
        newAllTicks += parts.at(i).toInt();
    }

    int idleTickDelta = newIdleTicks - idleTicks;
    int allTickDelta = newAllTicks - ticks;
    double load = 1.0 - ((double)idleTickDelta / allTickDelta);

    idleTicks = newIdleTicks;
    ticks = newAllTicks;

    return load;
}

/*
 * Wait in semi-busy loop until system load drops below IDLE_TRESHOLD
 */
void waitForIdle(int pollInterval) {
    double load = 1.0;
    QDateTime startTime = QDateTime::currentDateTime();
    getSystemLoad();
    while (load > IDLE_TRESHOLD) {
        qDebug() << __PRETTY_FUNCTION__ << "Waiting system to calm down. Wait time:"
            << startTime.secsTo(QDateTime::currentDateTime()) << "seconds. Load:"
            << load * 100 << "\%";
        QTest::qWait(pollInterval);
        load = getSystemLoad();
    }
    qDebug() << __PRETTY_FUNCTION__ << "Done. Wait time:"
        << startTime.secsTo(QDateTime::currentDateTime()) << "seconds. Load:"
        << load * 100 << "\%";
}

bool waitSignal(QSignalSpy &spy, int msec)
{
    QTime timer;
    timer.start();
    while (timer.elapsed() < msec && spy.isEmpty())
        QCoreApplication::processEvents();

    return !spy.isEmpty();
}
