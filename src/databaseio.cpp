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

class QueryHelper {
public:
    typedef QPair<QByteArray,QVariant> Field;
    typedef QList<Field> FieldList;

    QueryHelper()
    {
    }

    static QSqlQuery insertQuery(QByteArray q, const FieldList &fields)
    {
        QByteArray fieldsStr, valuesStr;
        foreach (const Field &field, fields) {
            fieldsStr += field.first + ", ";
            valuesStr += ":" + field.first + ", ";
        }
        fieldsStr.chop(2);
        valuesStr.chop(2);
        q.replace(":fields", fieldsStr);
        q.replace(":values", valuesStr);

        // XXX
        QSqlQuery query = CommHistoryDatabase::prepare(q, DatabaseIOPrivate::instance()->connection());
        foreach (const Field &field, fields) 
            query.bindValue(QString::fromLatin1(":" + field.first), field.second);

        return query;
    }

    static QSqlQuery updateQuery(QByteArray q, const FieldList &fields)
    {
        QByteArray fieldsStr;
        foreach (const Field &field, fields)
            fieldsStr += field.first + "=:" + field.first + ", ";
        fieldsStr.chop(2);
        q.replace(":fields", fieldsStr);

        // XXX
        QSqlQuery query = CommHistoryDatabase::prepare(q, DatabaseIOPrivate::instance()->connection());

        foreach (const Field &field, fields)
            query.bindValue(QString::fromLatin1(":" + field.first), field.second);

        return query;
    }

    static FieldList eventFields(const Event &event, const Event::PropertySet &properties)
    {
        FieldList fields;

        foreach (Event::Property property, properties) {
            switch (property) {
                case Event::Type:
                    fields.append(QueryHelper::Field("type", event.type()));
                    break;
                case Event::StartTime:
                    fields.append(QueryHelper::Field("startTime", event.startTime().toTime_t()));
                    break;
                case Event::EndTime:
                    fields.append(QueryHelper::Field("endTime", event.endTime().toTime_t()));
                    break;
                case Event::Direction:
                    fields.append(QueryHelper::Field("direction", event.direction()));
                    break;
                case Event::IsDraft:
                    fields.append(QueryHelper::Field("isDraft", event.isDraft()));
                    break;
                case Event::IsRead:
                    fields.append(QueryHelper::Field("isRead", event.isRead()));
                    break;
                case Event::IsMissedCall:
                    fields.append(QueryHelper::Field("isMissedCall", event.isMissedCall()));
                    break;
                case Event::IsEmergencyCall:
                    fields.append(QueryHelper::Field("isEmergencyCall", event.isEmergencyCall()));
                    break;
                case Event::Status:
                    fields.append(QueryHelper::Field("status", event.status()));
                    break;
                case Event::BytesReceived:
                    fields.append(QueryHelper::Field("bytesReceived", event.bytesReceived()));
                    break;
                case Event::LocalUid:
                    fields.append(QueryHelper::Field("localUid", event.localUid()));
                    break;
                case Event::RemoteUid:
                    fields.append(QueryHelper::Field("remoteUid", event.remoteUid()));
                    break;
                case Event::ParentId:
                    fields.append(QueryHelper::Field("parentId", event.parentId()));
                    break;
                case Event::Subject:
                    fields.append(QueryHelper::Field("subject", event.subject()));
                    break;
                case Event::FreeText:
                    fields.append(QueryHelper::Field("freeText", event.freeText()));
                    break;
                case Event::GroupId:
                    fields.append(QueryHelper::Field("groupId", event.groupId() == -1 ? QVariant() : event.groupId()));
                    break;
                case Event::MessageToken:
                    fields.append(QueryHelper::Field("messageToken", event.messageToken()));
                    break;
                case Event::LastModified:
                    fields.append(QueryHelper::Field("lastModified", event.lastModified()));
                    break;
                case Event::FromVCardFileName:
                    fields.append(QueryHelper::Field("vCardFileName", event.fromVCardFileName()));
                    break;
                case Event::FromVCardLabel:
                    fields.append(QueryHelper::Field("vCardLabel", event.fromVCardLabel()));
                    break;
                case Event::IsDeleted:
                    fields.append(QueryHelper::Field("isDeleted", event.isDeleted()));
                    break;
                case Event::ReportDelivery:
                    fields.append(QueryHelper::Field("reportDelivery", event.reportDelivery()));
                    break;
                case Event::ValidityPeriod:
                    fields.append(QueryHelper::Field("validityPeriod", event.validityPeriod()));
                    break;
                case Event::ContentLocation:
                    fields.append(QueryHelper::Field("contentLocation", event.contentLocation()));
                    break;
                case Event::Cc:
                    fields.append(QueryHelper::Field("targetCc", event.ccList().join('\n')));
                    break;
                case Event::Bcc:
                    fields.append(QueryHelper::Field("targetBcc", event.bccList().join('\n')));
                    break;
                case Event::ReadStatus:
                    fields.append(QueryHelper::Field("readStatus", event.readStatus()));
                    break;
                case Event::ReportRead:
                    fields.append(QueryHelper::Field("reportRead", event.reportRead()));
                    break;
                case Event::ReportReadRequested:
                    fields.append(QueryHelper::Field("reportedReadRequested", event.reportReadRequested()));
                    break;
                case Event::MmsId:
                    fields.append(QueryHelper::Field("mmsId", event.mmsId()));
                    break;
                case Event::To:
                    fields.append(QueryHelper::Field("targetTo", event.toList().join('\n')));
                    break;
                /* Irrelevant properties from Event */
                case Event::Id:
                case Event::ContactId:
                case Event::ContactName:
                case Event::Contacts:
                    break;
                /* XXX Disabled properties (remove?) */
                case Event::EventCount:
                case Event::IsAction:
                case Event::Headers:
                case Event::MessageParts:
                case Event::Encoding:
                case Event::CharacterSet:
                case Event::Language:
                    break;
                default:
                    qWarning() << Q_FUNC_INFO << "Event field ignored:" << property;
                    break;
            }
        }

        return fields;
    }

    static FieldList groupFields(const Group &group, const Group::PropertySet &properties)
    {
        FieldList fields;

        foreach (Group::Property property, properties) {
            switch (property) {
                case Group::LocalUid:
                    fields.append(QueryHelper::Field("localUid", group.localUid()));
                    break;
                case Group::RemoteUids:
                    fields.append(QueryHelper::Field("remoteUids", group.remoteUids().join('\n')));
                    break;
                case Group::Type:
                    fields.append(QueryHelper::Field("type", group.chatType()));
                    break;
                case Group::ChatName:
                    fields.append(QueryHelper::Field("chatName", group.chatName()));
                    break;
                case Group::StartTime:
                    fields.append(QueryHelper::Field("startTime", group.startTime()));
                    break;
                case Group::EndTime:
                    fields.append(QueryHelper::Field("endTime", group.endTime()));
                    break;
                /* Ignored properties (not settable) */
                case Group::Id:
                case Group::TotalMessages:
                case Group::UnreadMessages:
                case Group::SentMessages:
                case Group::LastEventId:
                case Group::ContactId:
                case Group::ContactName:
                case Group::LastMessageText:
                case Group::LastVCardFileName:
                case Group::LastVCardLabel:
                case Group::LastEventType:
                case Group::LastEventStatus:
                case Group::LastModified:
                case Group::Contacts:
                    break;
                default:
                    qWarning() << Q_FUNC_INFO << "Group field ignored:" << property;
                    break;
            }
        }

        return fields;
    }
 };

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

    QueryHelper::FieldList fields = QueryHelper::eventFields(event, event.allProperties());
    QSqlQuery query = QueryHelper::insertQuery("INSERT INTO Events (:fields) VALUES (:values)", fields);

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
    bool re = true;
    if (query.next())
        d->readEventResult(query, e);
    else
        re = false;

    event = e;
    return re;
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
    QueryHelper::FieldList fields = QueryHelper::eventFields(event, event.modifiedProperties());
    QSqlQuery query = QueryHelper::updateQuery("UPDATE Events SET :fields WHERE id=:eventId", fields);
    query.bindValue(":eventId", event.id());

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return true;
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

bool DatabaseIO::addGroup(Group &group)
{
    if (group.localUid().isEmpty() || group.remoteUids().isEmpty()) {
        qWarning() << Q_FUNC_INFO << "No local/remote UIDs for new group";
        return false;
    }

    QueryHelper::FieldList fields = QueryHelper::groupFields(group, Group::allProperties());
    QSqlQuery query = QueryHelper::insertQuery("INSERT INTO Groups (:fields) VALUES (:values)", fields);

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
    "\n  FROM Events GROUP BY groupId ORDER BY endTime, id DESC LIMIT 1 "
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
    QueryHelper::FieldList fields = QueryHelper::groupFields(group, group.modifiedProperties());
    QSqlQuery query = QueryHelper::updateQuery("UPDATE Groups SET :fields WHERE id=:groupId", fields);
    query.bindValue(":groupId", group.id());

    if (!query.exec()) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    return true;
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
    QByteArray q = "DELETE FROM Events ";
    if (eventType != Event::UnknownType)
        q += "WHERE type=:eventType ";

    QSqlQuery query = CommHistoryDatabase::prepare(q, d->connection());
    if (eventType != Event::UnknownType)
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

