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

#include <qtcontacts-extensions.h>
#include <qtcontacts-extensions_impl.h>

#include <QContactOriginMetadata>
#include <qcontactoriginmetadata_impl.h>

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

#include <QEventLoop>
#include <QFile>
#include <QTextStream>

#include "eventmodel.h"
#include "groupmodel.h"
#include "singleeventmodel.h"
#include "event.h"
#include "recipient.h"
#include "common.h"
#include "commonutils.h"
#include "contactlistener.h"

QTCONTACTS_USE_NAMESPACE

using namespace QtContactsSqliteExtensions;

namespace {

static int contactNumber = 0;

QContactManager *createManager()
{
    QMap<QString, QString> parameters;
    QString envspec(QStringLiteral("org.nemomobile.contacts.sqlite"));
    parameters.insert(QString::fromLatin1("mergePresenceChanges"), QString::fromLatin1("false"));
    if (!envspec.isEmpty()) {
        qDebug() << "Using contact manager:" << envspec;
        return new QContactManager(envspec, parameters);
    }

    return new QContactManager;
}

QContactManager *manager()
{
    static QContactManager *manager = createManager();
    return manager;
}

QContact createTestContact(const QString &name, const QString &remoteUid, const QString &localUid, const QString &contactUri)
{
    QContact contact;

    QContactSyncTarget syncTarget;
    syncTarget.setSyncTarget(QLatin1String("commhistory-tests"));
    if (!contact.saveDetail(&syncTarget)) {
        qWarning() << "Unable to add sync target to contact:" << contactUri;
        return QContact();
    }

    if (!localUid.isEmpty() && !localUidComparesPhoneNumbers(localUid)) {
        // Create a metadata detail to link the contact with the account
        QContactOriginMetadata metadata;
        metadata.setGroupId(localUid);
        metadata.setId(remoteUid);
        metadata.setEnabled(true);
        if (!contact.saveDetail(&metadata)) {
            qWarning() << "Unable to add metadata to contact:" << contactUri;
            return QContact();
        }

        QContactOnlineAccount qcoa;
        qcoa.setValue(QContactOnlineAccount__FieldAccountPath, localUid);
        qcoa.setAccountUri(remoteUid);
        if (!contact.saveDetail(&qcoa)) {
            qWarning() << "Unable to add online account to contact:" << contactUri;
            return QContact();
        }
    } else {
        QContactPhoneNumber phoneNumberDetail;
        phoneNumberDetail.setNumber(remoteUid);
        if (!contact.saveDetail(&phoneNumberDetail)) {
            qWarning() << "Unable to add phone number to contact:" << contactUri;
            return QContact();
        }
    }

    QContactName nameDetail;
    nameDetail.setLastName(name);
    if (!contact.saveDetail(&nameDetail)) {
        qWarning() << "Unable to add name to contact:" << contactUri;
        return QContact();
    }

    return contact;
}

}

namespace CommHistory {

void waitForSignal(QObject *object, const char *signal)
{
    QEventLoop loop;
    QObject::connect(object, signal, &loop, SLOT(quit()));
    loop.exec();
}

}

using namespace CommHistory;

const int numWords = 23;
const char* msgWords[] = { "lorem","ipsum","dolor","sit","amet","consectetur",
    "adipiscing","elit","in","imperdiet","cursus","lacus","vitae","suscipit",
    "maecenas","bibendum","rutrum","dolor","at","hendrerit",":)",":P","OMG!!" };
quint64 allTicks = 0;
quint64 idleTicks = 0;

QSet<QContactId> addedContactIds;
QSet<int> addedEventIds;

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
    if (type == Event::CallEvent)
        event.setEndTime(when.addSecs(TESTCALL_SECS));
    else
        event.setEndTime(event.startTime());
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
        addedEventIds.insert(event.id());
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
    grp.setRecipients(RecipientList::fromUids(localUid, QStringList() << remoteUid));
    QSignalSpy ready(&groupModel, SIGNAL(groupsCommitted(QList<int>,bool)));
    QVERIFY(groupModel.addGroup(grp));

    QVERIFY(waitSignal(ready));
    QVERIFY(ready.first().at(1).toBool());
}

int addTestContact(const QString &name, const QString &remoteUid, const QString &localUid)
{
    QString contactUri = QString("<testcontact:%1>").arg(contactNumber++);

    QContact contact(createTestContact(name, remoteUid, localUid, contactUri));
    if (contact.isEmpty())
        return -1;

    if (!manager()->saveContact(&contact)) {
        qWarning() << "Unable to store contact:" << contactUri;
        return -1;
    }

    foreach (const QContactRelationship &relationship, manager()->relationships(QContactRelationship::Aggregates(), contact, QContactRelationship::Second)) {
        const QContactId &aggId = relationship.first().id();
        qDebug() << "********** contact id" << aggId;
        addedContactIds.insert(aggId);
        return internalContactId(aggId);
    }

    qWarning() << "Could not find aggregator";
    return internalContactId(contact.id());
}

QList<int> addTestContacts(const QList<QPair<QString, QPair<QString, QString> > > &details)
{
    QList<int> ids;
    QList<QContact> contacts;

    QList<QPair<QString, QPair<QString, QString> > >::const_iterator it = details.constBegin(), end = details.constEnd();
    for ( ; it != end; ++it) {
        QString contactUri = QString("<testcontact:%1>").arg(contactNumber++);

        QContact contact(createTestContact(it->first, it->second.first, it->second.second, contactUri));
        if (!contact.isEmpty())
            contacts.append(contact);
    }

    if (!manager()->saveContacts(&contacts)) {
        qWarning() << "Unable to store contacts";
        return ids;
    }

    QSet<QContactId> constituentIds;
    foreach (const QContact &contact, contacts) {
        constituentIds.insert(contact.id());
    }
    foreach (const QContactRelationship &relationship, manager()->relationships(QContactRelationship::Aggregates())) {
        if (constituentIds.contains(relationship.second().id())) {
            const QContactId &aggId = relationship.first().id();
            qDebug() << "********** contact id" << aggId;
            addedContactIds.insert(aggId);
            ids.append(internalContactId(aggId));
        }
    }

    return ids;
}

bool addTestContactAddress(int contactId, const QString &remoteUid, const QString &localUid)
{
    QContact existing = manager()->contact(apiContactId(contactId));
    if (internalContactId(existing.id()) != (unsigned)contactId) {
        qWarning() << "Could not retrieve contact:" << contactId;
        return false;
    }

    if (!localUidComparesPhoneNumbers(localUid)) {
        QContactOriginMetadata metadata = existing.detail<QContactOriginMetadata>();
        if (metadata.groupId().isEmpty()) {
            // Create a metadata detail to link the contact with the account
            metadata.setGroupId(localUid);
            metadata.setId(remoteUid);
            metadata.setEnabled(true);
            if (!existing.saveDetail(&metadata)) {
                qWarning() << "Unable to add metadata to contact:" << contactId;
                return false;
            }
        }

        QContactOnlineAccount qcoa;
        qcoa.setValue(QContactOnlineAccount__FieldAccountPath, localUid);
        qcoa.setAccountUri(remoteUid);
        if (!existing.saveDetail(&qcoa)) {
            qWarning() << "Unable to add online account to contact:" << contactId;
            return false;
        }
    } else {
        QContactPhoneNumber phoneNumberDetail;
        phoneNumberDetail.setNumber(remoteUid);
        if (!existing.saveDetail(&phoneNumberDetail)) {
            qWarning() << "Unable to add phone number to contact:" << contactId;
            return false;
        }
    }

    if (!manager()->saveContact(&existing)) {
        qWarning() << "Unable to store contact:" << contactId;
        return false;
    }

    return true;
}

void modifyTestContact(int id, const QString &name)
{
    qDebug() << Q_FUNC_INFO << id << name;

    QContact contact = manager()->contact(apiContactId(id));
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
    if (!manager()->removeContact(apiContactId(id))) {
        qWarning() << "error deleting contact:" << id;
    }
    addedContactIds.remove(apiContactId(id));
}

void cleanUpTestContacts()
{
    if (!addedContactIds.isEmpty()) {
        QString aggregatesType = QContactRelationship::Aggregates();

        foreach (const QContactRelationship &rel, manager()->relationships(aggregatesType)) {
            QContactId firstId = rel.first().id();
            QContactId secondId = rel.second().id();
            if (addedContactIds.contains(firstId)) {
                addedContactIds.insert(secondId);
            }
        }

        if (!manager()->removeContacts(addedContactIds.toList())) {
            qWarning() << "Unable to remove test contacts:" << addedContactIds;
        }

        addedContactIds.clear();
    }
}

void cleanupTestGroups()
{
    GroupModel groupModel;
    groupModel.enableContactChanges(false);
    groupModel.setQueryMode(EventModel::SyncQuery);
    if (!groupModel.getGroups()) {
        qCritical() << Q_FUNC_INFO << "groupModel::getGroups failed";
        return;
    }

    if (!groupModel.deleteAll())
        qCritical() << Q_FUNC_INFO << "groupModel::deleteAll failed";
}

void cleanupTestEvents()
{
    SingleEventModel model;

    QSet<int>::const_iterator it = addedEventIds.constBegin(), end = addedEventIds.constEnd();
    for ( ; it != end; ++it) {
        model.deleteEvent(*it);
    }

    addedEventIds.clear();
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
    cleanupTestGroups();
    cleanupTestEvents();
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
    if (parts.size() < 10) {
        qWarning() << __PRETTY_FUNCTION__ << "Invalid input from /proc/stat:" << line;
        return -1;
    }
    if (parts.at(0) != QStringLiteral("cpu")) {
        qWarning() << __PRETTY_FUNCTION__ << "Invalid input from /proc/stat:" << line;
        return -1;
    }

    int newIdleTicks = parts.at(4).toInt();
    int newAllTicks = 0;
    for (int i = 1; i < parts.count(); i++) {
        newAllTicks += parts.at(i).toInt();
    }

    quint64 idleTickDelta = newIdleTicks - idleTicks;
    quint64 allTickDelta = newAllTicks - allTicks;
    idleTicks = newIdleTicks;
    allTicks = newAllTicks;

    return 1.0 - ((double)idleTickDelta / allTickDelta);
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
