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
#include <QSqlQuery>

#include "commonutils.h"
#include "contactresolver.h"
#include "databaseio.h"
#include "databaseio_p.h"
#include "eventmodel.h"
#include "groupmanager.h"
#include "updatesemitter.h"
#include "group.h"
#include "event.h"
#include "constants.h"
#include "contactlistener.h"
#include "debug.h"

namespace {

bool initializeTypes()
{
    qRegisterMetaType<QList<CommHistory::Event> >();
    qRegisterMetaType<QList<CommHistory::Group> >();
    qRegisterMetaType<QList<int> >();
    return true;
}

const int defaultChunkSize = 50;

}

bool groupmanager_initialized = initializeTypes();

namespace CommHistory {

class DatabaseIO;
class UpdatesEmitter;

class GroupManagerPrivate : public QObject
{
    Q_OBJECT

    Q_DECLARE_PUBLIC(GroupManager)

public:
    typedef ContactListener::ContactAddress ContactAddress;

    GroupManager *q_ptr;

    GroupManagerPrivate(GroupManager *parent = 0);
    ~GroupManagerPrivate();

    bool groupMatchesFilter(const Group &group) const;

    void add(const Group &group);
    void addGroups(const QList<Group> &groups);

    void modifyInModel(Group &group, bool query = true);

    bool canFetchMore() const;

    bool commitTransaction(const QList<int> &groupIds);

    DatabaseIO* database();

public Q_SLOTS:
    void eventsAddedSlot(const QList<CommHistory::Event> &events);

    void groupsAddedSlot(const QList<CommHistory::Group> &addedGroups);

    void groupsUpdatedSlot(const QList<int> &groupIds);
    void groupsUpdatedFullSlot(const QList<CommHistory::Group> &groups);

    void groupsDeletedSlot(const QList<int> &groupIds);

    void slotContactUpdated(quint32 localId,
                            const QString &contactName,
                            const QList<ContactAddress> &contactAddresses);

    void slotContactRemoved(quint32 localId);

    void contactResolveFinished();

public:
    EventModel::QueryMode queryMode;
    int chunkSize;
    int firstChunkSize;
    int queryLimit;
    int queryOffset;
    bool isReady;
    QHash<int,GroupObject*> groups;

    QString filterLocalUid;
    QString filterRemoteUid;

    QThread *bgThread;

    QSharedPointer<ContactListener> contactListener;
    ContactResolver *contactResolver;
    bool resolveContacts;
    QSharedPointer<UpdatesEmitter> emitter;

    QList<Group> pendingResolve;
};

}

using namespace CommHistory;
typedef ContactListener::ContactAddress ContactAddress;

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
        , bgThread(0)
        , contactResolver(0)
        , resolveContacts(false)
{
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
}

GroupManagerPrivate::~GroupManagerPrivate()
{
}

bool GroupManagerPrivate::groupMatchesFilter(const Group &group) const
{
    return (filterLocalUid.isEmpty() || group.localUid() == filterLocalUid)
            && (filterRemoteUid.isEmpty() || group.recipients() == Recipient(group.localUid(), filterRemoteUid));
}

void GroupManagerPrivate::add(const Group &group)
{
    Q_Q(GroupManager);

    DEBUG() << __PRETTY_FUNCTION__ << ": added" << group.toString();

    GroupObject *go = new GroupObject(group, q);
    groups.insert(go->id(), go);
    emit q->groupAdded(go);
}

void GroupManagerPrivate::addGroups(const QList<Group> &groups)
{
    if (!groups.isEmpty()) {
        if (resolveContacts && queryMode != EventModel::SyncQuery) {
            if (!contactResolver) {
                contactResolver = new ContactResolver(this);
                connect(contactResolver, SIGNAL(finished()),
                        this, SLOT(contactResolveFinished()));
            }

            pendingResolve.append(groups);
            contactResolver->add(groups);
        } else {
            foreach (const Group &group, groups)
                add(group);
        }
    }
}

bool GroupManagerPrivate::commitTransaction(const QList<int> &groupIds)
{
    const bool success = database()->commit();
    emit q_ptr->groupsCommitted(groupIds, success);
    return success;
}

void GroupManagerPrivate::modifyInModel(Group &group, bool query)
{
    Q_Q(GroupManager);

    GroupObject *go = groups.value(group.id());
    if (!go)
        return;

    if (query) {
        Group newGroup;
        if (!database()->getGroup(group.id(), newGroup))
            return;
        go->set(newGroup);
    } else {
        go->copyValidProperties(group);
    }

    emit q->groupUpdated(go);
    DEBUG() << __PRETTY_FUNCTION__ << ": updated" << go->toString();
}

void GroupManagerPrivate::eventsAddedSlot(const QList<Event> &events)
{
    Q_Q(GroupManager);
    DEBUG() << __PRETTY_FUNCTION__ << events.count();

    foreach (const Event &event, events) {
        // statusmessages are not shown in group model
        if (event.type() == Event::StatusMessageEvent
            || event.type() == Event::ClassZeroSMSEvent) {
            continue;
        }

        GroupObject *go = groups.value(event.groupId());
        if (!go)
            continue;

        if (event.endTime() >= go->endTime()) {
            DEBUG() << __PRETTY_FUNCTION__ << ": updating group" << go->id();
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
            go->setLastEventIsDraft(event.isDraft());
            go->setStartTime(event.startTime());
            go->setEndTime(event.endTime());
        }

        if (!event.isRead())
            go->setUnreadMessages(go->unreadMessages() + 1);
        emit q->groupUpdated(go);
    }
}

void GroupManagerPrivate::groupsAddedSlot(const QList<CommHistory::Group> &addedGroups)
{
    DEBUG() << Q_FUNC_INFO << addedGroups.count();

    QList<Group> newGroups;

    foreach (Group group, addedGroups) {
        GroupObject *go = groups.value(group.id());

        // If the group has not been added to the model, add it.
        if (!go && !group.recipients().isEmpty() && groupMatchesFilter(group))
            newGroups.append(group);
    }

    addGroups(newGroups);
}

void GroupManagerPrivate::groupsUpdatedSlot(const QList<int> &groupIds)
{
    DEBUG() << __PRETTY_FUNCTION__ << groupIds.count();

    foreach (int id, groupIds) {
        Group g;
        g.setId(id);

        modifyInModel(g);
    }
}

void GroupManagerPrivate::groupsUpdatedFullSlot(const QList<CommHistory::Group> &groups)
{
    DEBUG() << __PRETTY_FUNCTION__ << groups.count();

    foreach (Group g, groups) {
        modifyInModel(g, false);
    }
}

void GroupManagerPrivate::groupsDeletedSlot(const QList<int> &groupIds)
{
    Q_Q(GroupManager);

    DEBUG() << __PRETTY_FUNCTION__ << groupIds.count();

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

bool GroupManagerPrivate::canFetchMore() const
{
    return false;
}

DatabaseIO* GroupManagerPrivate::database()
{
    return DatabaseIO::instance();
}

void GroupManagerPrivate::slotContactUpdated(quint32 localId,
                                           const QString &contactName,
                                           const QList<ContactAddress> &contactAddresses)
{
    Q_UNUSED(localId);
    Q_UNUSED(contactName);
    Q_UNUSED(contactAddresses);
#if 0
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
#endif
}

void GroupManagerPrivate::slotContactRemoved(quint32 localId)
{
    Q_UNUSED(localId);
#if 0
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
#endif
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

GroupObject *GroupManager::findGroup(const QString &localUid, const QString &remoteUid) const
{
    return findGroup(localUid, QStringList() << remoteUid);
}

GroupObject *GroupManager::findGroup(const QString &localUid, const QStringList &remoteUids) const
{
    RecipientList match = RecipientList::fromUids(localUid, remoteUids);
    foreach (GroupObject *g, d->groups) {
        if (g->localUid() == localUid && g->recipients() == match)
            return g;
    }

    return 0;
}

bool GroupManager::addGroup(Group &group)
{
    if (!d->database()->transaction())
        return false;

    if (!d->database()->addGroup(group)) {
        d->database()->rollback();
        return false;
    }

    if (!d->commitTransaction(QList<int>() << group.id()))
        return false;

    if (d->groupMatchesFilter(group))
        d->add(group);

    d->emitter->groupsAdded(QList<Group>() << group);

    return true;
}

bool GroupManager::addGroups(QList<Group> &groups)
{
    QList<int> addedIds;
    QList<Group> addedGroups;

    QMutableListIterator<Group> i(groups);

    if (!d->database()->transaction())
        return false;

    while (i.hasNext()) {
        Group &group = i.next();
        if (!d->database()->addGroup(group)) {
            d->database()->rollback();
            return false;
        }

        if (d->groupMatchesFilter(group))
            d->add(group);

        addedIds.append(group.id());
        addedGroups.append(group);
    }

    if (!d->commitTransaction(addedIds))
        return false;

    d->emitter->groupsAdded(addedGroups);
    return true;
}

bool GroupManager::modifyGroup(Group &group)
{
    DEBUG() << Q_FUNC_INFO << group.id();

    if (group.id() == -1) {
        qWarning() << __FUNCTION__ << "Group id not set";
        return false;
    }

    if (!d->database()->transaction())
        return false;

    if (group.lastModified() == QDateTime::fromTime_t(0)) {
         group.setLastModified(QDateTime::currentDateTime());
    }

    if (!d->database()->modifyGroup(group)) {
        d->database()->rollback();
        return false;
    }

    if (!d->commitTransaction(QList<int>() << group.id()))
        return false;

    emit d->emitter->groupsUpdatedFull(QList<Group>() << group);
    return true;
}

bool GroupManager::getGroups(const QString &localUid,
                           const QString &remoteUid)
{
    d->filterLocalUid = localUid;
    d->filterRemoteUid = remoteUid;
    d->isReady = false;

    if (!d->groups.isEmpty()) {
        foreach (GroupObject *go, d->groups)
            emit groupDeleted(go);
        qDeleteAll(d->groups);
        d->groups.clear();
    }

    QString queryOrder;
    if (d->queryLimit > 0)
        queryOrder += QString::fromLatin1("LIMIT %1 ").arg(d->queryLimit);
    if (d->queryOffset > 0)
        queryOrder += QString::fromLatin1("OFFSET %1 ").arg(d->queryOffset);

    QList<Group> results;
    if (!d->database()->getGroups(localUid, remoteUid, results, queryOrder))
        return false;

    d->addGroups(results);

    if (!d->isReady && d->pendingResolve.isEmpty()) {
        d->isReady = true;
        emit modelReady(true);
    }

    return true;
}

void GroupManagerPrivate::contactResolveFinished()
{
    Q_Q(GroupManager);

    QList<Group> results = pendingResolve;
    pendingResolve.clear();

    DEBUG() << "Finished resolving" << results.size() << "groups";

    foreach (const Group &g, results) {
        GroupObject *go = new GroupObject(g, q);
        DEBUG() << g.id() << g.recipients().debugString();
        groups.insert(g.id(), go);
        emit q->groupAdded(go);
    }

    if (!isReady) {
        isReady = true;
        emit q->modelReady(true);
    }
}

bool GroupManager::markAsReadGroup(int id)
{
    DEBUG() << Q_FUNC_INFO << id;

    if (!d->database()->transaction())
        return false;

    if (!d->database()->markAsReadGroup(id)) {
        d->database()->rollback();
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

void GroupManager::updateGroups(QList<Group> &groups)
{
    // no need to update d->groups
    // cause they will be updated on the emitted signal as well
    if (!groups.isEmpty())
        emit d->emitter->groupsUpdatedFull(groups);
}

bool GroupManager::deleteGroups(const QList<int> &groupIds)
{
    DEBUG() << Q_FUNC_INFO << groupIds;

    if (!d->database()->transaction())
        return false;

    if (!d->database()->deleteGroups(groupIds, d->bgThread)) {
        d->database()->rollback();
        return false;
    }

    if (!d->commitTransaction(groupIds))
        return false;

    emit d->emitter->groupsDeleted(groupIds);
    return true;
}

bool GroupManager::deleteAll()
{
    DEBUG() << Q_FUNC_INFO;

    QList<int> ids;
    foreach (GroupObject *group, d->groups) {
        ids << group->id();
    }

    if (ids.isEmpty())
        return true;

    return deleteGroups(ids);
}

bool GroupManager::canFetchMore() const
{
    return d->canFetchMore();
}

void GroupManager::fetchMore()
{
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
}

QThread* GroupManager::backgroundThread()
{
    return d->bgThread;
}

DatabaseIO& GroupManager::databaseIO()
{
    return *d->database();
}

bool GroupManager::resolveContacts() const
{
    return d->resolveContacts;
}

void GroupManager::setResolveContacts(bool enabled)
{
    if (d->resolveContacts == enabled)
        return;
    d->resolveContacts = enabled;

    if (d->resolveContacts && !d->contactListener) {
        d->contactListener = ContactListener::instance();
        connect(d->contactListener.data(),
                SIGNAL(contactUpdated(quint32, const QString&, const QList<ContactAddress>&)),
                d,
                SLOT(slotContactUpdated(quint32, const QString&, const QList<ContactAddress>&)));
        connect(d->contactListener.data(),
                SIGNAL(contactRemoved(quint32)),
                d,
                SLOT(slotContactRemoved(quint32)));
    } else if (!d->resolveContacts && d->contactListener) {
        disconnect(d->contactListener.data(), 0, d, 0);
        d->contactListener.clear();
    }

    emit resolveContactsChanged();
}

#include "groupmanager.moc"

