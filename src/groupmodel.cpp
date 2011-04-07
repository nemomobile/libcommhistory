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

#include <QtDBus/QtDBus>
#include <QDebug>

#include "commonutils.h"
#include "trackerio.h"
#include "trackerio_p.h"
#include "queryrunner.h"
#include "groupmodel.h"
#include "groupmodel_p.h"
#include "eventmodel.h"
#include "adaptor.h"
#include "group.h"
#include "event.h"
#include "constants.h"
#include "committingtransaction.h"
#include "contactlistener.h"

namespace {

bool groupLessThan(CommHistory::Group &a, CommHistory::Group &b)
{
    return a.endTime() > b.endTime(); // descending order
}

static const int defaultChunkSize = 50;
}

using namespace CommHistory;

uint GroupModelPrivate::modelSerial = 0;

GroupModelPrivate::GroupModelPrivate(GroupModel *model)
        : q_ptr(model)
        , queryMode(EventModel::AsyncQuery)
        , chunkSize(defaultChunkSize)
        , firstChunkSize(0)
        , queryLimit(0)
        , queryOffset(0)
        , isReady(true)
        , filterLocalUid(QString())
        , filterRemoteUid(QString())
        , queryRunner(0)
        , threadCanFetchMore(false)
        , bgThread(0)
        , m_pTracker(0)
        , contactChangesEnabled(true)
{
    qRegisterMetaType<QList<CommHistory::Event> >();
    qRegisterMetaType<QList<CommHistory::Group> >();

    new Adaptor(this);
    if (!QDBusConnection::sessionBus().registerObject(newObjectPath(), this)) {
        qWarning() << __FUNCTION__ << ": error registering object";
    }

    QDBusConnection::sessionBus().connect(
        QString(),
        QString(),
        COMM_HISTORY_SERVICE_NAME,
        EVENTS_ADDED_SIGNAL,
        this,
        SLOT(eventsAddedSlot(const QList<CommHistory::Event> &)));
    QDBusConnection::sessionBus().connect(
        QString(),
        QString(),
        COMM_HISTORY_SERVICE_NAME,
        GROUP_ADDED_SIGNAL,
        this,
        SLOT(groupAddedSlot(CommHistory::Group)));
    QDBusConnection::sessionBus().connect(
        QString(),
        QString(),
        COMM_HISTORY_SERVICE_NAME,
        GROUPS_UPDATED_SIGNAL,
        this,
        SLOT(groupsUpdatedSlot(const QList<int> &)));
    QDBusConnection::sessionBus().connect(
        QString(),
        QString(),
        COMM_HISTORY_SERVICE_NAME,
        GROUPS_UPDATED_FULL_SIGNAL,
        this,
        SLOT(groupsUpdatedFullSlot(const QList<CommHistory::Group> &)));
    QDBusConnection::sessionBus().connect(
        QString(),
        QString(),
        COMM_HISTORY_SERVICE_NAME,
        GROUPS_DELETED_SIGNAL,
        this,
        SLOT(groupsDeletedSlot(const QList<int> &)));

    resetQueryRunner();
}

GroupModelPrivate::~GroupModelPrivate()
{
    deleteQueryRunner();
}

void GroupModelPrivate::resetQueryRunner()
{
    deleteQueryRunner();

    queryRunner = new QueryRunner(tracker());

    connect(queryRunner,
            SIGNAL(groupsReceived(int, int, QList<CommHistory::Group>)),
            this,
            SLOT(groupsReceivedSlot(int, int, QList<CommHistory::Group>)));
    connect(queryRunner,
            SIGNAL(canFetchMoreChanged(bool)),
            this,
            SLOT(canFetchMoreChangedSlot(bool)));
    connect(queryRunner,
            SIGNAL(modelUpdated(bool)),
            this,
            SLOT(modelUpdatedSlot(bool)));

    if (bgThread) {
        qDebug() << Q_FUNC_INFO << "MOVE" << queryRunner;
        queryRunner->moveToThread(bgThread);
    }

    qDebug() << Q_FUNC_INFO << this << queryRunner;
}

void GroupModelPrivate::deleteQueryRunner()
{
    if (queryRunner) {
        qDebug() << Q_FUNC_INFO << "DELETE" << queryRunner;
        queryRunner->disconnect(this);
        queryRunner->deleteLater();
        queryRunner = 0;
    }
}

QString GroupModelPrivate::newObjectPath()
{
    return QString(QLatin1String("/CommHistoryGroupModel")) +
        QString::number(modelSerial++);
}

CommittingTransaction* GroupModelPrivate::commitTransaction(QList<int> groupIds)
{
    CommittingTransaction *t = tracker()->commit();

    if (t) {
        t->addSignal(false,
                     q_ptr,
                     "groupsCommitted",
                     Q_ARG(QList<int>, groupIds),
                     Q_ARG(bool, true));
        t->addSignal(true,
                     q_ptr,
                     "groupsCommitted",
                     Q_ARG(QList<int>, groupIds),
                     Q_ARG(bool, false));
    }

    return t;
}

void GroupModelPrivate::addToModel(Group &group)
{
    Q_Q(GroupModel);

    qDebug() << __PRETTY_FUNCTION__ << ": added" << group.toString();

    QModelIndex index;
    q->beginInsertRows(index, 0, 0);
    groups.prepend(group);
    q->endInsertRows();
}

void GroupModelPrivate::modifyInModel(Group &group, bool query)
{
    Q_Q(GroupModel);
    int id = group.id();
    for (int row = 0; row < groups.count(); row++) {
        Group g = groups.at(row);
        if (g.id() == id) {
            Group newGroup;
            if (query) {
                if (!tracker()->getGroup(id, newGroup)) {
                    return;
                }
            } else {
                newGroup = group;
            }

            // preserve contact info if necessary
            if (!newGroup.validProperties().contains(Group::Contacts)
                && g.validProperties().contains(Group::Contacts)) {
                newGroup.setContacts(g.contacts());
            }

            groups.replace(row, newGroup);

            emit q->dataChanged(q->index(row, 0),
                                q->index(row, GroupModel::NumberOfColumns - 1));

            // don't sort if it's the first group or last message date
            // hasn't changed
            if (row > 0 && g.endTime() != newGroup.endTime()) {
                emit q->layoutAboutToBeChanged();
                qSort(groups.begin(), groups.end(), groupLessThan);
                emit q->layoutChanged();
            }

            qDebug() << __PRETTY_FUNCTION__ << ": updated" << newGroup.toString();
            break;
        }
    }
}

void GroupModelPrivate::groupsReceivedSlot(int start,
                                           int end,
                                           QList<CommHistory::Group> result)
{
    Q_UNUSED(start);
    Q_UNUSED(end);

    Q_Q(GroupModel);
    qDebug() << __PRETTY_FUNCTION__ << ": read" << result.count() << "groups";

    if (result.count()) {
        q->beginInsertRows(QModelIndex(), q->rowCount(),
                           q->rowCount() + result.count() - 1);
        groups.append(result);
        q->endInsertRows();

        startContactListening();
    }
}

void GroupModelPrivate::modelUpdatedSlot(bool successful)
{
    Q_Q(GroupModel);

    isReady = true;
    emit q->modelReady(successful);
}

void GroupModelPrivate::executeQuery(const QString query)
{
    isReady = false;
    if (queryMode == EventModel::StreamedAsyncQuery) {
        queryRunner->setStreamedMode(true);
        queryRunner->setChunkSize(chunkSize);
        queryRunner->setFirstChunkSize(firstChunkSize);
    } else {
        // TODO: support this by query runner
        //if (queryLimit) query.limit(queryLimit);
        //if (queryOffset) query.offset(queryOffset);
    }
    qDebug() << Q_FUNC_INFO << this << queryRunner;
    queryRunner->runGroupQuery(query);

    if (queryMode == EventModel::SyncQuery) {
        QEventLoop loop;
        while (!isReady) {
            loop.processEvents(QEventLoop::WaitForMoreEvents);
        }
    }
}

void GroupModelPrivate::eventsAddedSlot(const QList<Event> &events)
{
    Q_Q(GroupModel);
    qDebug() << __PRETTY_FUNCTION__ << events.count();

    bool sortNeeded = false;
    QList<int> changedRows;
    foreach (const Event &event, events) {
        // drafts and statusmessages are not shown in group model
        if (event.isDraft()
            || event.type() == Event::StatusMessageEvent
            || event.type() == Event::ClassZeroSMSEvent) {
            continue;
        }

        Group g;
        int row;
        for (row = 0; row < groups.count(); row++) {
            if (groups.at(row).id() == event.groupId()) {
                g = groups.at(row);
                break;
            }
        }
        if (!g.isValid()) continue;

        sortNeeded = sortNeeded || !g.endTime().isValid();

        if (event.endTime() >= g.endTime()) {
            qDebug() << __PRETTY_FUNCTION__ << ": updating group" << g.id();
            g.setLastEventId(event.id());
            if(event.type() == Event::MMSEvent) {
                g.setLastMessageText(event.subject().isEmpty() ? event.freeText() : event.subject());
            } else {
                g.setLastMessageText(event.freeText());
            }
            g.setLastVCardFileName(event.fromVCardFileName());
            g.setLastVCardLabel(event.fromVCardLabel());
            g.setLastEventStatus(event.status());
            g.setLastEventType(event.type());
            g.setStartTime(event.startTime());
            g.setEndTime(event.endTime());

            sortNeeded = sortNeeded || row != 0;
        }

        bool found = false;
        QString phoneNumber = normalizePhoneNumber(event.remoteUid());
        if (!phoneNumber.isEmpty()) {
            foreach (QString uid, g.remoteUids()) {
                if (uid.endsWith(phoneNumber.right(CommHistory::phoneNumberMatchLength()))) {
                    found = true;
                    break;
                }
            }
        } else {
            found = g.remoteUids().contains(event.remoteUid());
        }

        if (!found) {
            QStringList uids = g.remoteUids() << event.remoteUid();
            // TODO for future improvement: have separate properties for
            // tpTargetId and remoteUids. Meanwhile, just use the first
            // id as target.
            g.setRemoteUids(uids);
        }

        g.setTotalMessages(g.totalMessages() + 1);
        if (!event.isRead()) {
            g.setUnreadMessages(g.unreadMessages() + 1);
        }
        if (event.direction() == Event::Outbound) {
            g.setSentMessages(g.sentMessages() + 1);
        }
        groups.replace(row, g);
        changedRows.append(row);
    }

    foreach (int row, changedRows) {
        emit q->dataChanged(q->index(row, 0),
                            q->index(row, GroupModel::NumberOfColumns - 1));
    }

    if (sortNeeded) {
        emit q->layoutAboutToBeChanged();
        qSort(groups.begin(), groups.end(), groupLessThan);
        emit q->layoutChanged();
    }
}

void GroupModelPrivate::groupAddedSlot(CommHistory::Group group)
{
    qDebug() << __PRETTY_FUNCTION__ << group.id() << group.localUid()
             << group.remoteUids() << group.chatName() << group.chatType();

    Group g;
    for (int i = 0; i < groups.count(); i++)
        if (groups.at(i).id() == group.id()) {
            g = groups.at(i);
        }

    if (!g.isValid()
        && (filterLocalUid.isEmpty() || group.localUid() == filterLocalUid)
        && (filterRemoteUid.isEmpty()
            || CommHistory::remoteAddressMatch(filterRemoteUid, group.remoteUids().first()))) {
        g = group;
        addToModel(g);
    }

    if (g.isValid()) {
        startContactListening();
        if (contactListener)
            contactListener->resolveContact(g.localUid(),
                                            g.remoteUids().first());
    }
}

void GroupModelPrivate::groupsUpdatedSlot(const QList<int> &groupIds)
{
    qDebug() << __PRETTY_FUNCTION__ << groupIds.count();

    foreach (int id, groupIds) {
        Group g;
        g.setId(id);

        modifyInModel(g);
    }
}

void GroupModelPrivate::groupsUpdatedFullSlot(const QList<CommHistory::Group> &groups)
{
    qDebug() << __PRETTY_FUNCTION__ << groups.count();

    foreach (Group g, groups) {
        modifyInModel(g, false);
    }
}

void GroupModelPrivate::groupsDeletedSlot(const QList<int> &groupIds)
{
    Q_Q(GroupModel);

    qDebug() << __PRETTY_FUNCTION__ << groupIds.count();

    // convert ids to indexes
    QList<int> indexes;
    for (int i = 0; i < groups.size(); i++) {
        if (groupIds.contains(groups[i].id())) {
            indexes.append(i);
        }
    }

    // ensure order
    qSort(indexes.begin(), indexes.end());
    // delete from end
    QList<int>::iterator i = indexes.end();
    while (i != indexes.begin()) {
        int end = *--i;
        // find end
        while (i != indexes.begin() && *i == (*(i - 1) + 1))
            --i;
        int start = *i;

        q->beginRemoveRows(QModelIndex(), start, end);
        for (int j = end; j >= start; --j)
            groups.removeAt(j);
        q->endRemoveRows();
    }
}

void GroupModelPrivate::canFetchMoreChangedSlot(bool canFetch)
{
    threadCanFetchMore = canFetch;
}

bool GroupModelPrivate::canFetchMore() const
{
    return threadCanFetchMore;
}

TrackerIO* GroupModelPrivate::tracker()
{
    return TrackerIO::instance();
}

void GroupModelPrivate::slotContactUpdated(quint32 localId,
                                           const QString &contactName,
                                           const QList< QPair<QString,QString> > &contactAddresses)
{
    Q_Q(GroupModel);

    for (int row = 0; row < groups.count(); row++) {

        Group group = groups.at(row);
        bool updatedGroup = false;
        // NOTE: this is value copy, modifications need to be saved to group
        QList<Event::Contact> resolvedContacts = group.contacts();

        // if we already keep track of this contact and the address is in the provided matching addresses list
        if (ContactListener::addressMatchesList(group.localUid(),
                                                group.remoteUids().first(),
                                                contactAddresses)) {

            // check if contact is already resolved and stored in group
            for (int i = 0; i < resolvedContacts.count(); i++) {

                // if yes, then just exchange the contactname
                if ((quint32)resolvedContacts.at(i).first == localId) {

                    // modify contacts list, save it to group later
                    resolvedContacts[i].second = contactName;
                    updatedGroup = true;
                    break;
                }
            }

            // if not yet in group, then add it there
            if (!updatedGroup) {

                // modify contacts list, save it to group later
                resolvedContacts << Event::Contact(localId, contactName);
            }

            updatedGroup = true;
        }

        // otherwise we either don't keep track of the contact
        // or the address was removed from the contact and that is why it's not in the provided matching addresses list
        else {

            // check if contact is already resolved and stored in group
            for (int i = 0; i < resolvedContacts.count(); i++) {

                // if yes, then remove it from there
                if ((quint32)resolvedContacts.at(i).first == localId) {

                    // modify contacts list, save it to group later
                    resolvedContacts.removeAt(i);
                    updatedGroup = true;
                    break;
                }
            }
        }

        if (updatedGroup) {

            group.setContacts(resolvedContacts);
            groups.replace(row, group);
            emit q->dataChanged(q->index(row, GroupModel::Contacts),
                                q->index(row, GroupModel::Contacts));
        }
    }
}

void GroupModelPrivate::slotContactRemoved(quint32 localId)
{
    Q_Q(GroupModel);

    for (int row = 0; row < groups.count(); row++) {

        Group group = groups.at(row);
        bool updatedGroup = false;
        // NOTE: this is value copy, modifications need to be saved to group
        QList<Event::Contact> resolvedContacts = group.contacts();

        // check if contact is already resolved and stored in group
        for (int i = 0; i < resolvedContacts.count(); i++) {

            // if yes, then remove it from there
            if ((quint32)resolvedContacts.at(i).first == localId) {

                // modify contacts list, save it to group later
                resolvedContacts.removeAt(i);
                updatedGroup = true;
                break;
            }
        }

        if (updatedGroup) {

            group.setContacts(resolvedContacts);
            groups.replace(row, group);

            emit q->dataChanged(q->index(row, GroupModel::Contacts),
                                q->index(row, GroupModel::Contacts));
        }
    }
}

void GroupModelPrivate::startContactListening()
{
    if (contactChangesEnabled && !contactListener) {
        contactListener = ContactListener::instance();
        connect(contactListener.data(),
                SIGNAL(contactUpdated(quint32, const QString&, const QList<QPair<QString,QString> >&)),
                this,
                SLOT(slotContactUpdated(quint32, const QString&, const QList<QPair<QString,QString> >&)));
        connect(contactListener.data(),
                SIGNAL(contactRemoved(quint32)),
                this,
                SLOT(slotContactRemoved(quint32)));
    }
}

GroupModel::GroupModel(QObject *parent)
    : QAbstractTableModel(parent),
      d(new GroupModelPrivate(this))
{
}

GroupModel::~GroupModel()
{
    delete d;
    d = 0;
}

void GroupModel::setQueryMode(EventModel::QueryMode mode)
{
    d->queryMode = mode;
}

void GroupModel::setChunkSize(uint size)
{
    d->chunkSize = size;
}

void GroupModel::setFirstChunkSize(uint size)
{
    d->firstChunkSize = size;
}

void GroupModel::setLimit(int limit)
{
    d->queryLimit = limit;
}

void GroupModel::setOffset(int offset)
{
    d->queryOffset = offset;
}

int GroupModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return d->groups.count();
}

int GroupModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return NumberOfColumns;
}

QVariant GroupModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(role);

    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= d->groups.count() || index.row() < 0) {
        return QVariant();
    }

    Group group = d->groups.at(index.row());

    if (role == GroupRole) {
        return QVariant::fromValue(group);
    }

    QVariant var;
    switch (index.column()) {
        case GroupId:
            var = QVariant::fromValue(group.id());
            break;
        case LocalUid:
            var = QVariant::fromValue(group.localUid());
            break;
        case RemoteUids:
            var = QVariant::fromValue(group.remoteUids());
            break;
        case ChatName:
            var = QVariant::fromValue(group.chatName());
            break;
        case EndTime:
            var = QVariant::fromValue(group.endTime());
            break;
        case TotalMessages:
            var = QVariant::fromValue(group.totalMessages());
            break;
        case UnreadMessages:
            var = QVariant::fromValue(group.unreadMessages());
            break;
        case SentMessages:
            var = QVariant::fromValue(group.sentMessages());
            break;
        case LastEventId:
            var = QVariant::fromValue(group.lastEventId());
            break;
        case Contacts:
            var = QVariant::fromValue(group.contacts());
            break;
        case LastMessageText:
            var = QVariant::fromValue(group.lastMessageText());
            break;
        case LastVCardFileName:
            var = QVariant::fromValue(group.lastVCardFileName());
            break;
        case LastVCardLabel:
            var = QVariant::fromValue(group.lastVCardLabel());
            break;
        case LastEventType:
            var = QVariant::fromValue((int)group.lastEventType());
            break;
        case LastEventStatus:
            var = QVariant::fromValue((int)group.lastEventStatus());
            break;
        case LastModified:
            var = QVariant::fromValue(group.lastModified());
            break;
        case StartTime:
            var = QVariant::fromValue(group.startTime());
            break;
        default:
            qDebug() << "Group::data: invalid column id??" << index.column();
            break;
    }

    return var;
}

Group GroupModel::group(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Group();
    }

    if (index.row() >= d->groups.count() || index.row() < 0) {
        return Group();
    }

    return d->groups.at(index.row());
}

QVariant GroupModel::headerData(int section,
                                Qt::Orientation orientation,
                                int role) const
{
    QVariant var;

    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        QString name;
        switch (section) {
            case GroupId:
                name = QLatin1String("id");
                break;
            case LocalUid:
                name = QLatin1String("local_uid");
                break;
            case RemoteUids:
                name = QLatin1String("remote_uids");
                break;
            case ChatName:
                name = QLatin1String("chat_name");
                break;
            case EndTime:
                name = QLatin1String("end_time");
                break;
            case TotalMessages:
                name = QLatin1String("total_messages");
                break;
            case UnreadMessages:
                name = QLatin1String("unread_messages");
                break;
            case SentMessages:
                name = QLatin1String("sent_messages");
                break;
            case LastEventId:
                name = QLatin1String("last_event_id");
                break;
            case Contacts:
                name = QLatin1String("contacts");
                break;
            case LastMessageText:
                name = QLatin1String("last_message_text");
                break;
            case LastVCardFileName:
                name = QLatin1String("last_vcard_filename");
                break;
            case LastVCardLabel:
                name = QLatin1String("last_vcard_label");
                break;
            case LastEventType:
                name = QLatin1String("last_event_type");
                break;
            case LastEventStatus:
                name = QLatin1String("last_event_status");
                break;
            case LastModified:
                name = QLatin1String("last_modified");
                break;
            case StartTime:
                name = QLatin1String("start_time");
                break;
            default:
                break;
        }
        var = QVariant::fromValue(name);
    }

    return var;
}

bool GroupModel::addGroup(Group &group)
{
    d->tracker()->transaction();
    if (!d->tracker()->addGroup(group)) {
        d->tracker()->rollback();
        return false;
    }

    if (!d->commitTransaction(QList<int>() << group.id()))
        return false;

    if ((d->filterLocalUid.isEmpty() || group.localUid() == d->filterLocalUid)
        && (d->filterRemoteUid.isEmpty()
            || CommHistory::remoteAddressMatch(d->filterRemoteUid, group.remoteUids().first()))) {
        QModelIndex index;
        beginInsertRows(index, 0, 0);
        d->groups.prepend(group);
        endInsertRows();
    }

    emit d->groupAdded(group);

    return true;
}

bool GroupModel::modifyGroup(Group &group)
{
    d->tracker()->transaction();

    if (group.id() == -1) {
        qWarning() << __FUNCTION__ << "Group id not set";
        d->tracker()->rollback();
        return false;
    }

    if (group.lastModified() == QDateTime::fromTime_t(0)) {
         group.setLastModified(QDateTime::currentDateTime());
    }

    if (!d->tracker()->modifyGroup(group)) {
        d->tracker()->rollback();
        return false;
    }

    CommittingTransaction *t = d->commitTransaction(QList<int>() << group.id());
    if (t)
        t->addSignal(false,
                     d,
                     "groupsUpdatedFull",
                     Q_ARG(QList<CommHistory::Group>, QList<Group>() << group));

    return t != 0;
}

bool GroupModel::getGroups(const QString &localUid,
                           const QString &remoteUid)
{
    d->filterLocalUid = localUid;
    d->filterRemoteUid = remoteUid;

    if (!d->groups.isEmpty()) {
        reset();
        d->groups.clear();
    }

    QSparqlQuery query(TrackerIOPrivate::prepareGroupQuery(localUid, remoteUid));
    d->executeQuery(query.preparedQueryText());

    return true;
}

bool GroupModel::markAsReadGroup(int id)
{
    qDebug() << Q_FUNC_INFO << id;

    d->tracker()->transaction();

    if (!d->tracker()->markAsReadGroup(id)) {
        d->tracker()->rollback();
        return false;
    }

    if (!d->commitTransaction(QList<int>() << id))
        return false;

    Group group;

    foreach (Group g, d->groups) {
        if (g.id() == id) {
            group = g;
            group.setUnreadMessages(0);
            break;
        }
    }

    if (group.isValid())
        emit d->groupsUpdatedFull(QList<Group>() << group);
    else
        emit d->groupsUpdated(QList<int>() << id);

    return true;
}

void GroupModel::updateGroups(QList<Group> &groups)
{
    // no need to update d->groups
    // cause they will be updated on the emitted signal as well
    if (!groups.isEmpty())
        emit d->groupsUpdatedFull(groups);
}

bool GroupModel::deleteGroups(const QList<int> &groupIds, bool deleteMessages)
{
    qDebug() << Q_FUNC_INFO << groupIds;

    d->tracker()->transaction();
    if (!d->tracker()->deleteGroups(groupIds, deleteMessages, d->bgThread)) {
        d->tracker()->rollback();
        return false;
    }
    CommittingTransaction *t = d->commitTransaction(groupIds);

    if (t)
        t->addSignal(false, d, "groupsDeleted", Q_ARG(QList<int>, groupIds));

    return t != 0;
}

bool GroupModel::deleteAll()
{
    qDebug() << Q_FUNC_INFO;

    QList<int> ids;
    foreach (Group group, d->groups) {
        ids << group.id();
    }

    if(ids.size() <= 0) {
        return true;
    }

    return deleteGroups(ids, true);
}

bool GroupModel::canFetchMore(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    if (!d->queryRunner)
        return false;

    return d->canFetchMore();
}

void GroupModel::fetchMore(const QModelIndex &parent)
{
    Q_UNUSED(parent);

    if (d->queryRunner)
        d->queryRunner->fetchMore();
}

bool GroupModel::isReady() const
{
    return d->isReady;
}

EventModel::QueryMode GroupModel::queryMode() const
{
    return d->queryMode;
}

uint GroupModel::chunkSize() const
{
    return d->chunkSize;
}

uint GroupModel::firstChunkSize() const
{
    return d->firstChunkSize;
}

int GroupModel::limit() const
{
    return d->queryLimit;
}

int GroupModel::offset() const
{
    return d->queryOffset;
}

void GroupModel::setBackgroundThread(QThread *thread)
{
    d->bgThread = thread;

    qDebug() << Q_FUNC_INFO << thread;

    d->resetQueryRunner();
}

QThread* GroupModel::backgroundThread()
{
    return d->bgThread;
}

TrackerIO& GroupModel::trackerIO()
{
    return *d->tracker();
}

void GroupModel::enableContactChanges(bool enabled)
{
    d->contactChangesEnabled = enabled;
}
