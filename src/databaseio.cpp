/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: John Brooks <john.brooks@jollamobile.com>
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

#include "databaseio_p.h"
#include "databaseio.h"
#include "commhistorydatabase.h"
#include "group.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

using namespace CommHistory;

Q_GLOBAL_STATIC(DatabaseIO, databaseIO)

DatabaseIO *DatabaseIO::instance()
{
    return databaseIO();
}

DatabaseIO::DatabaseIO()
    : d(new DatabaseIOPrivate(this))
{
}

DatabaseIO::~DatabaseIO()
{
}

DatabaseIOPrivate *DatabaseIOPrivate::instance()
{
    return DatabaseIO::instance()->d;
}

DatabaseIOPrivate::DatabaseIOPrivate(DatabaseIO *p)
    : q(p)
{
}

DatabaseIOPrivate::~DatabaseIOPrivate()
{
}

QSqlDatabase &DatabaseIOPrivate::connection()
{
    if (!m_pConnection.isValid())
        m_pConnection = CommHistoryDatabase::open("commhistory");

    return m_pConnection;
}

QSqlQuery DatabaseIOPrivate::createQuery()
{
    return QSqlQuery(connection());
}

static const char *addEventQuery =
    "\n INSERT INTO Events ("
    "\n  type,"
    "\n  startTime,"
    "\n  endTime,"
    "\n  direction,"
    "\n  isDraft,"
    "\n  isRead,"
    "\n  isMissedCall,"
    "\n  isEmergencyCall,"
    "\n  status,"
    "\n  bytesReceived,"
    "\n  localUid,"
    "\n  remoteUid,"
    "\n  parentId,"
    "\n  subject,"
    "\n  freeText,"
    "\n  groupId,"
    "\n  messageToken,"
    "\n  lastModified,"
    "\n  vCardFileName,"
    "\n  vCardLabel,"
    "\n  isDeleted,"
    "\n  reportDelivery,"
    "\n  validityPeriod,"
    "\n  contentLocation,"
    "\n  messageParts,"
    "\n  targetCc,"
    "\n  targetBcc,"
    "\n  readStatus,"
    "\n  reportRead,"
    "\n  reportedReadRequested,"
    "\n  mmsId,"
    "\n  targetTo"
    "\n ) VALUES ("
    "\n  :type,"
    "\n  :startTime,"
    "\n  :endTime,"
    "\n  :direction,"
    "\n  :isDraft,"
    "\n  :isRead,"
    "\n  :isMissedCall,"
    "\n  :isEmergencyCall,"
    "\n  :status,"
    "\n  :bytesReceived,"
    "\n  :localUid,"
    "\n  :remoteUid,"
    "\n  :parentId,"
    "\n  :subject,"
    "\n  :freeText,"
    "\n  :groupId,"
    "\n  :messageToken,"
    "\n  :lastModified,"
    "\n  :vCardFileName,"
    "\n  :vCardLabel,"
    "\n  :isDeleted,"
    "\n  :reportDelivery,"
    "\n  :validityPeriod,"
    "\n  :contentLocation,"
    "\n  :messageParts,"
    "\n  :targetCc,"
    "\n  :targetBcc,"
    "\n  :readStatus,"
    "\n  :reportRead,"
    "\n  :reportedReadRequested,"
    "\n  :mmsId,"
    "\n  :targetTo"
    "\n )";

bool DatabaseIO::addEvent(Event &event)
{
    if (event.type() == Event::UnknownType) {
        qWarning() << Q_FUNC_INFO << "Event type not set";
        return false;
    }

    if (event.direction() == Event::UnknownDirection) {
        qWarning() << Q_FUNC_INFO << "Event direction not set";
        return false;
    }

    if (event.groupId() == -1 && event.type() != Event::CallEvent) {
        qWarning() << Q_FUNC_INFO << "Group id not set";
        return false;
    }

    if (event.id() != -1)
        qWarning() << Q_FUNC_INFO << "Adding event with an ID set. ID will be ignored.";

    QSqlQuery query = CommHistoryDatabase::prepare(addEventQuery, d->connection());
    query.bindValue(":type", event.type());
    query.bindValue(":startTime", event.startTime().toTime_t());
    query.bindValue(":endTime", event.endTime().toTime_t());
    query.bindValue(":direction", event.direction());
    query.bindValue(":isDraft", event.isDraft());
    query.bindValue(":isRead", event.isRead());
    query.bindValue(":isMissedCall", event.isMissedCall());
    query.bindValue(":isEmergencyCall", event.isEmergencyCall());
    query.bindValue(":status", event.status());
    query.bindValue(":bytesReceived", event.bytesReceived());
    query.bindValue(":localUid", event.localUid());
    query.bindValue(":remoteUid", event.remoteUid());
    query.bindValue(":parentId", event.parentId());
    query.bindValue(":subject", event.subject());
    query.bindValue(":freeText", event.freeText());
    query.bindValue(":groupId", event.groupId());
    query.bindValue(":messageToken", event.messageToken());
    query.bindValue(":lastModified", event.lastModified());
    query.bindValue(":vCardFileName", event.fromVCardFileName());
    query.bindValue(":vCardLabel", event.fromVCardLabel());
    query.bindValue(":isDeleted", event.isDeleted());
    query.bindValue(":reportDelivery", event.reportDelivery());
    query.bindValue(":validityPeriod", event.validityPeriod());
    query.bindValue(":contentLocation", event.contentLocation());
    // XXX disabled
    query.bindValue(":messageParts", QVariant());
    query.bindValue(":targetCc", event.ccList().join('\n'));
    query.bindValue(":targetBcc", event.bccList().join('\n'));
    query.bindValue(":readStatus", event.readStatus());
    query.bindValue(":reportedReadRequested", event.reportReadRequested());
    query.bindValue(":mmsId", event.mmsId());
    query.bindValue(":targetTo", event.toList().join('\n'));

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    event.setId(query.lastInsertId().toInt());
    return true;
}

static const char *baseEventQuery =
    "\n SELECT "
    "\n Events.id, "
    "\n Events.type, "
    "\n Events.startTime, "
    "\n Events.endTime, "
    "\n Events.direction, "
    "\n Events.isDraft, "
    "\n Events.isRead, "
    "\n Events.isMissedCall, "
    "\n Events.isEmergencyCall, "
    "\n Events.status, "
    "\n Events.bytesReceived, "
    "\n Events.localUid, "
    "\n Events.remoteUid, "
    "\n Events.parentId, "
    "\n Events.subject, "
    "\n Events.freeText, "
    "\n Events.groupId, "
    "\n Events.messageToken, "
    "\n Events.lastModified, "
    "\n Events.vCardFileName, "
    "\n Events.vCardLabel, "
    "\n Events.isDeleted, "
    "\n Events.reportDelivery, "
    "\n Events.validityPeriod, "
    "\n Events.contentLocation, "
    "\n Events.messageParts, "
    "\n Events.targetCc, "
    "\n Events.targetBcc, "
    "\n Events.readStatus, "
    "\n Events.reportRead, "
    "\n Events.reportedReadRequested, "
    "\n Events.mmsId, "
    "\n Events.targetTo "
    "\n FROM Events ";

QString DatabaseIOPrivate::eventQueryBase() 
{
    return QLatin1String(baseEventQuery);
}

void DatabaseIOPrivate::readEventResult(QSqlQuery &query, Event &event)
{
    event.setId(query.value(0).toInt());
    event.setType(static_cast<Event::EventType>(query.value(1).toInt()));
    event.setStartTime(QDateTime::fromTime_t(query.value(2).toUInt()));
    event.setEndTime(QDateTime::fromTime_t(query.value(3).toUInt()));
    event.setDirection(static_cast<Event::EventDirection>(query.value(4).toInt()));
    event.setIsDraft(query.value(5).toBool());
    event.setIsRead(query.value(6).toBool());
    event.setIsMissedCall(query.value(7).toBool());
    event.setIsEmergencyCall(query.value(8).toBool());
    event.setStatus(static_cast<Event::EventStatus>(query.value(9).toInt()));
    event.setBytesReceived(query.value(10).toInt());
    event.setLocalUid(query.value(11).toString());
    event.setRemoteUid(query.value(12).toString());
    event.setParentId(query.value(13).toInt());
    event.setSubject(query.value(14).toString());
    event.setFreeText(query.value(15).toString());
    event.setGroupId(query.value(16).toInt());
    event.setMessageToken(query.value(17).toString());
    event.setLastModified(QDateTime::fromTime_t(query.value(18).toUInt()));
    event.setFromVCard(query.value(19).toString(), query.value(20).toString());
    event.setDeleted(query.value(21).toBool());
    event.setReportDelivery(query.value(22).toBool());
    event.setValidityPeriod(query.value(23).toInt());
    event.setContentLocation(query.value(24).toString());
    // Message parts are much more complicated
    //event.setMessageParts(query.value(25).toString());
    event.setCcList(QStringList() << query.value(26).toString().split('\n'));
    event.setBccList(QStringList() << query.value(27).toString().split('\n'));
    event.setReadStatus(static_cast<Event::EventReadStatus>(query.value(28).toInt()));
    event.setReportRead(query.value(29).toBool());
    event.setReportReadRequested(query.value(30).toBool());
    event.setMmsId(query.value(31).toString());
    event.setToList(QStringList() << query.value(32).toString().split('\n'));

    // contacts
    // event count
    // encoding, character set, language
}

bool DatabaseIO::getEvent(int id, Event &event)
{
    QByteArray q = baseEventQuery;
    q += "\n WHERE Events.id = :eventId LIMIT 1";

    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":eventId", id);

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    Event e;
    if (query.next())
        d->readEventResult(query, e);

    event = e;
    return true;
}

bool DatabaseIO::getEventByMessageToken(const QString &token, Event &event)
{
    QByteArray q = baseEventQuery;
    q += "\n WHERE Events.messageToken = :messageToken LIMIT 1";

    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":messageToken", token);

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    Event e;
    if (query.next())
        d->readEventResult(query, e);

    event = e;
    return true;
}

bool DatabaseIO::getEventByMmsId(const QString &mmsId, int groupId, Event &event)
{
    QByteArray q = baseEventQuery;
    q += "\n WHERE Events.mmsId = :mmsId AND Events.groupId = :groupId LIMIT 1";

    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":mmsId", mmsId);
    query.bindValue(":groupId", groupId);

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    Event e;
    if (query.next())
        d->readEventResult(query, e);

    event = e;
    return true;
}

bool DatabaseIO::modifyEvent(Event &event)
{
    Q_UNUSED(event);
    qDebug() << Q_FUNC_INFO << "stub";
    return false;
}

bool DatabaseIO::moveEvent(Event &event, int groupId)
{
    static const char *q = "UPDATE Events SET groupId=:groupId WHERE id=:id";
    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":groupId", groupId);
    query.bindValue(":id", event.id());

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    event.setGroupId(groupId);
    return true;
}

bool DatabaseIO::deleteEvent(Event &event, QThread *backgroundThread)
{
    Q_UNUSED(backgroundThread);

    static const char *q = "DELETE FROM Events WHERE id=:id";
    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":id", event.id());

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return true;
}

static const char *addGroupQuery =
    "\n INSERT INTO Groups ("
    "\n  localUid, "
    "\n  remoteUids, "
    "\n  type, "
    "\n  chatName, "
    "\n  startTime, "
    "\n  endTime)"
    "\n VALUES ("
    "\n  :localUid, "
    "\n  :remoteUids, "
    "\n  :type, "
    "\n  :chatName, "
    "\n  :startTime, "
    "\n  :endTime);";

bool DatabaseIO::addGroup(Group &group)
{
    QSqlQuery query = CommHistoryDatabase::prepare(addGroupQuery, d->connection());

    query.bindValue(":localUid", group.localUid());
    query.bindValue(":remoteUids", group.remoteUids().join('\n'));
    query.bindValue(":type", group.chatType());
    query.bindValue(":chatName", group.chatName());
    query.bindValue(":startTime", group.startTime());
    query.bindValue(":endTime", group.endTime());

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    group.setId(query.lastInsertId().toInt());
    return true;
}

/* Read the result from a groups query into a Group.
 * The order of fields must match those queries.
 */
void DatabaseIOPrivate::readGroupResult(QSqlQuery &query, Group &group)
{
    group.setId(query.value(0).toInt());
    group.setLocalUid(query.value(1).toString());
    group.setRemoteUids(query.value(2).toString().split('\n'));
    group.setChatType(static_cast<Group::ChatType>(query.value(3).toInt()));
    group.setChatName(query.value(4).toString());
    group.setStartTime(QDateTime::fromTime_t(query.value(5).toUInt()));
    group.setEndTime(QDateTime::fromTime_t(query.value(6).toUInt()));
    group.setTotalMessages(query.value(7).toInt());
    group.setUnreadMessages(query.value(8).toInt());
    group.setSentMessages(query.value(9).toInt());
    group.setLastEventId(query.value(10).toInt());
    group.setLastMessageText(query.value(11).toString());
    group.setLastVCardFileName(query.value(12).toString());
    group.setLastVCardLabel(query.value(13).toString());
    group.setLastEventType(static_cast<Event::EventType>(query.value(14).toInt()));
    group.setLastEventStatus(static_cast<Event::EventStatus>(query.value(15).toInt()));

    // lastModified
    // contacts
}

static const char *baseGroupQuery =
    "\n SELECT "
    "\n Groups.id, "
    "\n Groups.localUid, "
    "\n Groups.remoteUids, "
    "\n Groups.type, "
    "\n Groups.chatName, "
    "\n Groups.startTime, "
    "\n Groups.endTime, "
    "\n COUNT(Events.id), "
    "\n COUNT(NULLIF(Events.isRead = 0, 0)), "
    "\n COUNT(NULLIF(Events.direction = 2, 0)), "
    "\n LastEvent.id, "
    "\n LastEvent.freeText, "
    "\n LastEvent.vCardFileName, "
    "\n LastEvent.vCardLabel, "
    "\n LastEvent.type, "
    "\n LastEvent.status "
    "\n FROM Groups "
    "\n LEFT JOIN Events ON (Events.groupId = Groups.id) "
    "\n LEFT JOIN ("
    "\n  SELECT "
    "\n   id, "
    "\n   groupId, "
    "\n   freeText, "
    "\n   vCardFileName, "
    "\n   vCardLabel, "
    "\n   type, "
    "\n   status "
    "\n  FROM Events GROUP BY groupId ORDER BY endTime DESC LIMIT 1 "
    "\n ) AS LastEvent ON (LastEvent.groupId = Groups.id) ";

bool DatabaseIO::getGroup(int id, Group &group)
{
    QByteArray q = baseGroupQuery;
    q += "\n WHERE Groups.id = :groupId GROUP BY Groups.id LIMIT 1";

    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":groupId", id);

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    Group g;
    if (query.next())
        d->readGroupResult(query, g);

    group = g;
    return true;
}

bool DatabaseIO::getGroups(const QString &localUid, const QString &remoteUid, QList<Group> &result)
{
    QByteArray q = baseGroupQuery;
    if (!localUid.isEmpty() || !remoteUid.isEmpty()) {
        q += " WHERE ";
        if (!localUid.isEmpty()) {
            q += "Groups.localUid = :localUid ";
            if (!remoteUid.isEmpty())
                q += "AND ";
        }
        if (!remoteUid.isEmpty())
            q += "Groups.remoteUids = :remoteUid";
    }
    q += "GROUP BY Groups.id";

    QSqlQuery query = CommHistoryDatabase::prepare(q.data(), d->connection());
    if (!localUid.isEmpty())
        query.bindValue(":localUid", localUid);
    if (!remoteUid.isEmpty())
        query.bindValue(":remoteUid", remoteUid);

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    result.clear();
    while (query.next()) {
        Group g;
        d->readGroupResult(query, g);
        result.append(g);
    }

    return true;
}

bool DatabaseIO::modifyGroup(Group &group)
{
    Q_UNUSED(group);
    qDebug() << Q_FUNC_INFO << "stub";
    return false;
}

bool DatabaseIO::deleteGroup(int groupId, QThread *backgroundThread)
{
    return deleteGroups(QList<int>() << groupId, backgroundThread);
}

static inline QByteArray joinNumberList(const QList<int> &list)
{
    QByteArray re;
    foreach (int i, list) {
        if (!re.isEmpty())
            re += ',';
        re += QByteArray::number(i);
    }
    return re;
}

bool DatabaseIO::deleteGroups(QList<int> groupIds, QThread *backgroundThread)
{
    Q_UNUSED(backgroundThread);

    // Events are deleted via SQL foreign keys
    QByteArray q = "DELETE FROM Groups WHERE id IN (" + joinNumberList(groupIds) + ")";
    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return true;
}

bool DatabaseIO::totalEventsInGroup(int groupId, int &totalEvents)
{
    static const char *q = "SELECT COUNT(id) FROM Events WHERE groupId=:groupId";
    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":groupId", groupId);

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    if (query.next()) {
        totalEvents = query.value(0).toInt();
        return true;
    }

    return false;
}

bool DatabaseIO::markAsReadGroup(int groupId)
{
    static const char *q = "UPDATE Events SET isRead=1 WHERE groupId=:groupId";
    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":groupId", groupId);

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return true;
}

bool DatabaseIO::markAsRead(const QList<int> &eventIds)
{
    QByteArray q = "UPDATE Events SET isRead=1 WHERE id IN (";
    q += joinNumberList(eventIds) + ")";

    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return true;
}

bool DatabaseIO::markAsReadAll(Event::EventType eventType)
{
    static const char *q = "UPDATE Events SET isRead=1 WHERE type=:eventType";
    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":eventType", eventType);

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return true;
}

bool DatabaseIO::deleteAllEvents(Event::EventType eventType)
{
    static const char *q = "DELETE FROM Events WHERE type=:eventType";
    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    query.bindValue(":eventType", eventType);

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return true;
}

void DatabaseIO::transaction()
{
    if (!d->connection().transaction()) {
        qWarning() << "Failed to start transaction";
        qWarning() << d->connection().lastError();
    }
}

bool DatabaseIO::commit()
{
    bool re = d->connection().commit();
    if (!re) {
        qWarning() << "Failed to commit transaction";
        qWarning() << d->connection().lastError();
    }
    return re;
}

bool DatabaseIO::rollback()
{
    bool re = d->connection().rollback();
    if (!re) {
        qWarning() << "Failed to rollback transaction";
        qWarning() << d->connection().lastError();
    }
    return re;
}

QString DatabaseIOPrivate::makeCallGroupURI(const CommHistory::Event &event)
{
    QString callGroupRemoteId;
    QString number = normalizePhoneNumber(event.remoteUid());
    if (number.isEmpty()) {
        callGroupRemoteId = event.remoteUid();
    } else {
        // keep dial string in group uris for separate history entries
        callGroupRemoteId = makeShortNumber(event.remoteUid(), NormalizeFlagKeepDialString);
    }

    QString videoSuffix;
    if (event.isVideoCall())
        videoSuffix = "!video";

    return QString(QLatin1String("callgroup:%1!%2%3"))
        .arg(event.localUid())
        .arg(callGroupRemoteId)
        .arg(videoSuffix);
}

