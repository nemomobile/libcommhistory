/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include <QtDBus/QtDBus>
#include <QDebug>

#include "commonutils.h"
#include "trackerio.h"
#include "trackerio_p.h"
#include "queryrunner.h"
#include "eventmodel.h"
#include "groupmanager.h"
#include "groupmanager_p.h"
#include "updatesemitter.h"
#include "group.h"
#include "event.h"
#include "constants.h"
#include "committingtransaction.h"
#include "contactlistener.h"

namespace {
static const int defaultChunkSize = 50;

static const int maxAddGroupsSize = 25;
}

using namespace CommHistory;

GroupManagerPrivate::GroupManagerPrivate(GroupManager *manager)
        : q_ptr(manager)
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
    qRegisterMetaType<QList<int> >();

    emitter = UpdatesEmitter::instance();

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
        GROUPS_ADDED_SIGNAL,
        this,
        SLOT(groupsAddedSlot(const QList<CommHistory::Group> &)));
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

GroupManagerPrivate::~GroupManagerPrivate()
{
    deleteQueryRunner();
}

void GroupManagerPrivate::resetQueryRunner()
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

void GroupManagerPrivate::deleteQueryRunner()
{
    if (queryRunner) {
        qDebug() << Q_FUNC_INFO << "DELETE" << queryRunner;
        queryRunner->disconnect(this);
        queryRunner->deleteLater();
        queryRunner = 0;
    }
}

CommittingTransaction* GroupManagerPrivate::commitTransaction(QList<int> groupIds)
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

void GroupManagerPrivate::modifyInModel(Group &group, bool query)
{
    Q_Q(GroupManager);
    GroupObject *go = groups.value(group.id());
    if (!go)
        return;

    if (query) {
        Group newGroup;
        if (!tracker()->getGroup(group.id(), newGroup))
            return;

        // preserve contact info if necessary
        if (!newGroup.validProperties().contains(Group::Contacts)
            && go->validProperties().contains(Group::Contacts)) {
            newGroup.setContacts(go->contacts());
        }
        go->set(newGroup);
    } else {
        go->copyValidProperties(group);
    }

    emit q->groupUpdated(go);
    qDebug() << __PRETTY_FUNCTION__ << ": updated" << go->toString();
}

void GroupManagerPrivate::groupsReceivedSlot(int start,
                                           int end,
                                           QList<CommHistory::Group> result)
{
    Q_UNUSED(start);
    Q_UNUSED(end);

    Q_Q(GroupManager);
    qDebug() << __PRETTY_FUNCTION__ << ": read" << result.count() << "groups";

    foreach (Group g, result) {
        GroupObject *go = new GroupObject(g, q);
        groups.insert(g.id(), go);
        emit q->groupAdded(go);
    }
}

void GroupManagerPrivate::modelUpdatedSlot(bool successful)
{
    Q_Q(GroupManager);

    isReady = true;
    emit q->modelReady(successful);
}

void GroupManagerPrivate::executeQuery(const QString query)
{
    isReady = false;
    QString finalQuery(query);
    if (queryMode == EventModel::StreamedAsyncQuery) {
        queryRunner->setStreamedMode(true);
        queryRunner->setChunkSize(chunkSize);
        queryRunner->setFirstChunkSize(firstChunkSize);
    } else {
        if (queryLimit)
            finalQuery.append(QLatin1String(" LIMIT ") + QString::number(queryLimit));
        if (queryOffset)
            finalQuery.append(QLatin1String(" OFFSET ") + QString::number(queryOffset));
    }
    qDebug() << Q_FUNC_INFO << this << queryRunner;
    queryRunner->runGroupQuery(finalQuery);

    if (queryMode == EventModel::SyncQuery) {
        QEventLoop loop;
        while (!isReady) {
            loop.processEvents(QEventLoop::WaitForMoreEvents);
        }
    }
}

void GroupManagerPrivate::eventsAddedSlot(const QList<Event> &events)
{
    Q_Q(GroupManager);
    qDebug() << __PRETTY_FUNCTION__ << events.count();

    foreach (const Event &event, events) {
        // drafts and statusmessages are not shown in group model
        if (event.isDraft()
            || event.type() == Event::StatusMessageEvent
            || event.type() == Event::ClassZeroSMSEvent) {
            continue;
        }

        GroupObject *go = groups.value(event.groupId());
        if (!go)
            continue;

        if (event.startTime() >= go->startTime()) {
            qDebug() << __PRETTY_FUNCTION__ << ": updating group" << go->id();
            go->setLastEventId(event.id());
            if (event.type() == Event::MMSEvent) {
                go->setLastMessageText(event.subject().isEmpty() ? event.freeText() : event.subject());
            } else {
                go->setLastMessageText(event.freeText());
            }
            go->setLastVCardFileName(event.fromVCardFileName());
            go->setLastVCardLabel(event.fromVCardLabel());
            go->setLastEventStatus(event.status());
            go->setLastEventType(event.type());
            go->setStartTime(event.startTime());
            go->setEndTime(event.endTime());

            if ((event.type() == Event::SMSEvent || event.type() == Event::MMSEvent) &&
                go->remoteUids().first() != event.remoteUid()) {

                qDebug() << __PRETTY_FUNCTION__ << "Update group remote UIDs";
                QStringList updatedUids;
                foreach (const QString& uid, go->remoteUids()) {
                    if (CommHistory::remoteAddressMatch(uid, event.remoteUid())) {
                        updatedUids << event.remoteUid();
                    } else {
                        updatedUids << uid;
                    }
                }
                go->setRemoteUids(updatedUids);
            }
        }

        bool found = false;
        QString phoneNumber = normalizePhoneNumber(event.remoteUid());
        if (!phoneNumber.isEmpty()) {
            foreach (QString uid, go->remoteUids()) {
                if (uid.endsWith(phoneNumber.right(CommHistory::phoneNumberMatchLength()))) {
                    found = true;
                    break;
                }
            }
        } else {
            found = go->remoteUids().contains(event.remoteUid());
        }

        if (!found) {
            QStringList uids = go->remoteUids() << event.remoteUid();
            // TODO for future improvement: have separate properties for
            // tpTargetId and remoteUids. Meanwhile, just use the first
            // id as target.
            go->setRemoteUids(uids);
        }

        go->setTotalMessages(go->totalMessages() + 1);
        if (!event.isRead()) {
            go->setUnreadMessages(go->unreadMessages() + 1);
        }
        if (event.direction() == Event::Outbound) {
            go->setSentMessages(go->sentMessages() + 1);
        }

        emit q->groupUpdated(go);
    }
}

void GroupManagerPrivate::groupsAddedSlot(const QList<CommHistory::Group> &addedGroups)
{
    Q_Q(GroupManager);
    qDebug() << Q_FUNC_INFO << addedGroups.count();

    foreach (Group group, addedGroups) {
        GroupObject *go = groups.value(group.id());

        // If the group has not been added to the model, add it.
        if (!go
            && (filterLocalUid.isEmpty() || group.localUid() == filterLocalUid)
            && !group.remoteUids().isEmpty()
            && (filterRemoteUid.isEmpty()
                || CommHistory::remoteAddressMatch(filterRemoteUid, group.remoteUids().first()))) {
            go = new GroupObject(group, q);
            groups.insert(group.id(), go);
            emit q->groupAdded(go);
        }

        if (go) {
            // Start contact resolving if we are interested listening contacts
            // and the contacts are not yet being resolved.
            startContactListening();
            if (contactListener && !go->remoteUids().isEmpty())
                contactListener->resolveContact(go->localUid(),
                                                go->remoteUids().first());
        }
    }
}

void GroupManagerPrivate::groupsUpdatedSlot(const QList<int> &groupIds)
{
    qDebug() << __PRETTY_FUNCTION__ << groupIds.count();

    foreach (int id, groupIds) {
        Group g;
        g.setId(id);

        modifyInModel(g);
    }
}

void GroupManagerPrivate::groupsUpdatedFullSlot(const QList<CommHistory::Group> &groups)
{
    qDebug() << __PRETTY_FUNCTION__ << groups.count();

    foreach (Group g, groups) {
        modifyInModel(g, false);
    }
}

void GroupManagerPrivate::groupsDeletedSlot(const QList<int> &groupIds)
{
    Q_Q(GroupManager);

    qDebug() << __PRETTY_FUNCTION__ << groupIds.count();

    foreach (int id, groupIds) {
        GroupObject *go = groups.value(id);
        if (!go)
            continue;

        q->groupDeleted(go); 
        emit go->groupDeleted();
        go->deleteLater();
        groups.remove(id);
    }
}

void GroupManagerPrivate::canFetchMoreChangedSlot(bool canFetch)
{
    threadCanFetchMore = canFetch;
}

bool GroupManagerPrivate::canFetchMore() const
{
    return threadCanFetchMore;
}

TrackerIO* GroupManagerPrivate::tracker()
{
    return TrackerIO::instance();
}

void GroupManagerPrivate::slotContactUpdated(quint32 localId,
                                           const QString &contactName,
                                           const QList< QPair<QString,QString> > &contactAddresses)
{
    Q_Q(GroupManager);

    foreach (GroupObject *group, groups) {
        bool updatedGroup = false;
        // NOTE: this is value copy, modifications need to be saved to group
        QList<Event::Contact> resolvedContacts = group->contacts();

        // if we already keep track of this contact and the address is in the provided matching addresses list
        if (ContactListener::addressMatchesList(group->localUid(),
                                                group->remoteUids().first(),
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
            group->setContacts(resolvedContacts);
            emit q->groupUpdated(group);
        }
    }
}

void GroupManagerPrivate::slotContactRemoved(quint32 localId)
{
    Q_Q(GroupManager);

    foreach (GroupObject *group, groups) {
        bool updatedGroup = false;
        // NOTE: this is value copy, modifications need to be saved to group
        QList<Event::Contact> resolvedContacts = group->contacts();

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
            group->setContacts(resolvedContacts);
            emit q->groupUpdated(group);
        }
    }
}

void GroupManagerPrivate::slotContactSettingsChanged(const QHash<QString, QVariant> &changedSettings)
{
    Q_UNUSED(changedSettings);
    Q_Q(GroupManager);

    if (!groups.isEmpty())
        q->getGroups(filterLocalUid, filterRemoteUid);
}

void GroupManagerPrivate::startContactListening()
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

        connect(contactListener.data(),
                SIGNAL(contactSettingsChanged(const QHash<QString, QVariant> &)),
                this,
                SLOT(slotContactSettingsChanged(const QHash<QString, QVariant> &)));
    }
}

GroupManager::GroupManager(QObject *parent)
    : QObject(parent),
      d(new GroupManagerPrivate(this))
{
}

GroupManager::~GroupManager()
{
    delete d;
    d = 0;
}

void GroupManager::setQueryMode(EventModel::QueryMode mode)
{
    d->queryMode = mode;
}

void GroupManager::setChunkSize(int size)
{
    d->chunkSize = size;
}

void GroupManager::setFirstChunkSize(int size)
{
    d->firstChunkSize = size;
}

void GroupManager::setLimit(int limit)
{
    d->queryLimit = limit;
}

void GroupManager::setOffset(int offset)
{
    d->queryOffset = offset;
}

GroupObject *GroupManager::group(int groupId) const
{
    return d->groups.value(groupId);
}

void GroupManagerPrivate::add(Group &group)
{
    Q_Q(GroupManager);

    qDebug() << __PRETTY_FUNCTION__ << ": added" << group.toString();

    GroupObject *go = new GroupObject(group, q);
    groups.insert(go->id(), go);
    emit q->groupAdded(go);
}

bool GroupManager::addGroup(Group &group)
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
        d->add(group);
    }

    d->emitter->groupsAdded(QList<Group>() << group);

    return true;
}

bool GroupManager::addGroups(QList<Group> &groups)
{
    QList<int> addedIds;
    QList<Group> addedGroups;

    QMutableListIterator<Group> i(groups);

    while (i.hasNext()) {
        d->tracker()->transaction();

        while (addedGroups.size() < maxAddGroupsSize && i.hasNext()) {
            Group &group = i.next();
            if (!d->tracker()->addGroup(group)) {
                d->tracker()->rollback();
                return false;
            }

            if ((d->filterLocalUid.isEmpty() || group.localUid() == d->filterLocalUid)
                && (d->filterRemoteUid.isEmpty()
                    || CommHistory::remoteAddressMatch(d->filterRemoteUid, group.remoteUids().first()))) {
                d->add(group);
            }

            addedIds.append(group.id());
            addedGroups.append(group);
        }

        d->commitTransaction(addedIds);
        d->emitter->groupsAdded(addedGroups);
        addedIds.clear();
        addedGroups.clear();
    }

    return true;
}

#if 0
bool GroupManager::modifyGroup(Group &group)
{
    qDebug() << Q_FUNC_INFO << group.id();

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
                     d->emitter,
                     "groupsUpdatedFull",
                     Q_ARG(QList<CommHistory::Group>, QList<Group>() << group));

    return t != 0;
}
#endif

bool GroupManager::getGroups(const QString &localUid,
                           const QString &remoteUid)
{
    d->filterLocalUid = localUid;
    d->filterRemoteUid = remoteUid;

    if (!d->groups.isEmpty()) {
        foreach (GroupObject *go, d->groups)
            emit groupDeleted(go);
        qDeleteAll(d->groups);
        d->groups.clear();
    }

    d->startContactListening();

    QSparqlQuery query(TrackerIOPrivate::prepareGroupQuery(localUid, remoteUid));
    d->executeQuery(query.preparedQueryText());

    return true;
}

bool GroupManager::markAsReadGroup(int id)
{
    qDebug() << Q_FUNC_INFO << id;

    d->tracker()->transaction();

    if (!d->tracker()->markAsReadGroup(id)) {
        d->tracker()->rollback();
        return false;
    }

    if (!d->commitTransaction(QList<int>() << id))
        return false;

    GroupObject *group = 0;
    foreach (GroupObject *g, d->groups) {
        if (g->id() == id) {
            group = g;
            group->setUnreadMessages(0);
            break;
        }
    }

    if (group)
        emit d->emitter->groupsUpdatedFull(QList<Group>() << group->toGroup());
    else
        emit d->emitter->groupsUpdated(QList<int>() << id);

    return true;
}

#if 0
void GroupManager::updateGroups(QList<Group> &groups)
{
    // no need to update d->groups
    // cause they will be updated on the emitted signal as well
    if (!groups.isEmpty())
        emit d->groupsUpdatedFull(groups);
}
#endif

bool GroupManager::deleteGroups(const QList<int> &groupIds, bool deleteMessages)
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

bool GroupManager::deleteAll()
{
    qDebug() << Q_FUNC_INFO;

    QList<int> ids;
    foreach (GroupObject *group, d->groups) {
        ids << group->id();
    }

    if (ids.isEmpty())
        return true;

    return deleteGroups(ids, true);
}

bool GroupManager::canFetchMore() const
{
    if (!d->queryRunner)
        return false;

    return d->canFetchMore();
}

void GroupManager::fetchMore()
{
    if (d->queryRunner)
        d->queryRunner->fetchMore();
}

QList<GroupObject*> GroupManager::groups() const
{
    return d->groups.values();
}

bool GroupManager::isReady() const
{
    return d->isReady;
}

EventModel::QueryMode GroupManager::queryMode() const
{
    return d->queryMode;
}

int GroupManager::chunkSize() const
{
    return d->chunkSize;
}

int GroupManager::firstChunkSize() const
{
    return d->firstChunkSize;
}

int GroupManager::limit() const
{
    return d->queryLimit;
}

int GroupManager::offset() const
{
    return d->queryOffset;
}

void GroupManager::setBackgroundThread(QThread *thread)
{
    d->bgThread = thread;

    qDebug() << Q_FUNC_INFO << thread;

    d->resetQueryRunner();
}

QThread* GroupManager::backgroundThread()
{
    return d->bgThread;
}

TrackerIO& GroupManager::trackerIO()
{
    return *d->tracker();
}

void GroupManager::enableContactChanges(bool enabled)
{
    d->contactChangesEnabled = enabled;
}
