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

#include <QSparqlConnection>
#include <QSparqlResult>
#include <QSparqlQuery>
#include <QSparqlError>

#include <QContact>
#include <QContactManager>
#include <QContactDetail>
#include <QContactDetailFilter>
#include <QContactIntersectionFilter>
#include <QContactRelationshipFilter>
#include <QContactUnionFilter>

#include <QContactName>
#include <QContactNickname>
#include <QContactOnlineAccount>
#include <QContactPhoneNumber>
#include <QContactSyncTarget>

#include <QFile>
#include <QTextStream>

#include "eventmodel.h"
#include "groupmodel.h"
#include "callmodel.h"
#include "event.h"
#include "common.h"
#include "trackerio.h"
#include "commonutils.h"
#include "contactlistener.h"

#include "qcontacttpmetadata_p.h"

#ifdef USING_QTPIM
QTCONTACTS_USE_NAMESPACE
typedef QContactId QContactIdType;
#else
QTM_USE_NAMESPACE
typedef QContactLocalId QContactIdType;
#endif

namespace {
static int contactNumber = 0;

QContactManager *createManager()
{
    QString envspec(QLatin1String(qgetenv("NEMO_CONTACT_MANAGER")));
    if (!envspec.isEmpty()) {
        qDebug() << "Using contact manager:" << envspec;
        return new QContactManager(envspec);
    }

    return new QContactManager;
}

QContactManager *manager()
{
    static QContactManager *manager = createManager();
    return manager;
}
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
    event.setEndTime(when.addSecs(100));
    event.setLocalUid(account);
    if (remoteUid.isEmpty()) {
        event.setRemoteUid(type == Event::SMSEvent ? "555123456" : "td@localhost");
    } else if (remoteUid == "<hidden>") {
        event.setRemoteUid(QString());
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

    QVERIFY(waitSignal(ready));
    QVERIFY(ready.first().at(1).toBool());
}

int addTestContact(const QString &name, const QString &remoteUid, const QString &localUid)
{
    QString contactUri = QString("<testcontact:%1>").arg(contactNumber++);

    QContact contact;

    QContactSyncTarget syncTarget;
    syncTarget.setSyncTarget(QLatin1String("commhistory-tests"));
    if (!contact.saveDetail(&syncTarget)) {
        qWarning() << "Unable to add sync target to contact:" << contactUri;
        return -1;
    }

    if (!localUid.isEmpty()) {
        // Create a metadata detail to link the contact with the account
        QContactTpMetadata metadata;
        metadata.setContactId(remoteUid);
        metadata.setAccountId(localUid);
        metadata.setAccountEnabled(true);
        if (!contact.saveDetail(&metadata)) {
            qWarning() << "Unable to add metadata to contact:" << contactUri;
            return -1;
        }
    }

    QString normal = CommHistory::normalizePhoneNumber(remoteUid);
    if (normal.isEmpty()) {
        QContactOnlineAccount qcoa;
        qcoa.setValue(QContactOnlineAccount__FieldAccountPath, localUid);
        qcoa.setAccountUri(remoteUid);
        if (!contact.saveDetail(&qcoa)) {
            qWarning() << "Unable to add online account to contact:" << contactUri;
            return -1;
        }
    } else {
        QContactPhoneNumber phoneNumberDetail;
        phoneNumberDetail.setNumber(remoteUid);
        if (!contact.saveDetail(&phoneNumberDetail)) {
            qWarning() << "Unable to add phone number to contact:" << contactUri;
            return -1;
        }
    }

    QContactName nameDetail;
    nameDetail.setLastName(name);
    if (!contact.saveDetail(&nameDetail)) {
        qWarning() << "Unable to add name to contact:" << contactUri;
        return -1;
    }

    if (!manager()->saveContact(&contact)) {
        qWarning() << "Unable to store contact:" << contactUri;
        return -1;
    }

    // We should return the aggregated instance of this contact
    QContactRelationshipFilter filter;
    filter.setRelatedContactRole(QContactRelationship::Second);

#ifdef USING_QTPIM
    filter.setRelatedContact(contact);
    filter.setRelationshipType(QContactRelationship::Aggregates());
#else
    filter.setRelatedContactId(contact.id());
    filter.setRelationshipType(QContactRelationship::Aggregates);
#endif

    foreach (const QContactIdType &id, manager()->contactIds(filter)) {
        qDebug() << "********** contact id" << id;
        return ContactListener::internalContactId(id);
    }

    qWarning() << "Could not find aggregator";
    return ContactListener::internalContactId(contact);
}

void modifyTestContact(int id, const QString &name)
{
    qDebug() << Q_FUNC_INFO << id << name;

    QContact contact = manager()->contact(ContactListener::apiContactId(id));
    if (!contact.isEmpty()) {
        qWarning() << "unable to retrieve contact:" << id;
        return;
    }

    QContactName nameDetail = contact.detail<QContactName>();
    nameDetail.setLastName(name);
    if (!contact.saveDetail(&nameDetail)) {
        qWarning() << "Unable to add name to contact:" << id;
        return;
    }

    if (!manager()->saveContact(&contact)) {
        qWarning() << "Unable to store contact:" << id;
        return;
    }
}

void deleteTestContact(int id)
{
    if (!manager()->removeContact(ContactListener::apiContactId(id))) {
        qWarning() << "error deleting contact:" << id;
    }
}

void cleanUpTestContacts()
{
    qDebug() << Q_FUNC_INFO;
    QString query("DELETE { ?r a rdfs:Resource } WHERE { GRAPH <commhistory-tests> { ?r a rdfs:Resource } }");
    QScopedPointer<QSparqlConnection> conn(new QSparqlConnection(QLatin1String("QTRACKER_DIRECT")));
    QScopedPointer<QSparqlResult> result(conn->exec(QSparqlQuery(query,
                                                                 QSparqlQuery::DeleteStatement)));
    result->waitForFinished();
    if (result->hasError()) {
        qWarning() << "error deleting contacts:" << result->lastError().message();
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
        qWarning() << Q_FUNC_INFO << "startTime:" << e1.startTime().toString() << e2.startTime().toString();
        return false;
    }
    if (e1.endTime().toTime_t() != e2.endTime().toTime_t()) {
        qWarning() << Q_FUNC_INFO << "endTime:" << e1.endTime().toString() << e2.endTime().toString();
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
    if (e1.type() == Event::CallEvent && e1.isVideoCall() != e2.isVideoCall()) {
        qWarning() << Q_FUNC_INFO << "isVideoCall:" << e1.isVideoCall() << e2.isVideoCall();
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
    if (e1.headers() != e2.headers()) {
        qWarning() << Q_FUNC_INFO << "headers:" << e1.headers() << e2.headers();
        return false;
    }
    if (e1.toList() != e2.toList()) {
        qWarning() << Q_FUNC_INFO << "toList:" << e1.toList() << e2.toList();
        return false;
    }

//    QCOMPARE(e1.messageToken(), e2.messageToken());
    return true;
}

void deleteAll()
{
    qDebug() << __FUNCTION__ << "- Deleting all";

    cleanUpTestContacts();

    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    groupModel.setQueryMode(EventModel::SyncQuery);
    if (!groupModel.getGroups()) {
        qCritical() << Q_FUNC_INFO << "getGroups failed";
        return;
    }

    if (!groupModel.deleteAll())
        qCritical() << Q_FUNC_INFO << "deleteAll failed";

    CallModel callModel;
    callModel.enableContactChanges(false);
    callModel.setQueryMode(EventModel::SyncQuery);
    if (!callModel.getEvents(CallModel::SortByContact)) {
        qCritical() << Q_FUNC_INFO << "callModel::getEvents failed";
        return;
    }

    if (!callModel.deleteAll())
        qCritical() << Q_FUNC_INFO << "callModel::deleteAll failed";
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
    while (timer.elapsed() < msec && spy.isEmpty()) {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }

    return !spy.isEmpty();
}

void waitWithDeletes(int msec)
{
    QTime timer;
    timer.start();
    while (timer.elapsed() < msec) {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
}
