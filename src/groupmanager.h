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

#ifndef COMMHISTORY_GROUPMANAGER_H
#define COMMHISTORY_GROUPMANAGER_H

#include "groupobject.h"
#include "libcommhistoryexport.h"
#include "eventmodel.h"

namespace CommHistory {

class GroupManagerPrivate;
class TrackerIO;

/*!
 * \class GroupModel
 *
 * Model for event groups (conversations) in the commhistory database.
 * Contains rows of Group data. Call getGroups() to fill the model.
 * Dig into a conversation by either calling getEvents(), or
 * creating a new EventModel and filtering it by group.
 * Group data is automatically updated when new events arrive.
 * Use the Qt rowsInserted(), dataChanged() etc. signals to
 * monitor changes.
 */
class LIBCOMMHISTORY_EXPORT GroupManager : public QObject
{
    Q_OBJECT

public:
    GroupManager(QObject *parent = 0);
    virtual ~GroupManager();

    Q_PROPERTY(CommHistory::EventModel::QueryMode queryMode READ queryMode WRITE setQueryMode)
    EventModel::QueryMode queryMode() const;
    /*!
     * Set query mode. See EventModel::setQueryMode().
     */
    void setQueryMode(EventModel::QueryMode mode);

    Q_PROPERTY(int chunkSize READ chunkSize WRITE setChunkSize)
    int chunkSize() const;
    /*!
     * Set chunk size (number of groups to fetch) for asynchronous and
     * streamed queries.
     *
     * \param size Chunk size.
     */
    void setChunkSize(int size);

    Q_PROPERTY(int firstChunkSize READ firstChunkSize WRITE setFirstChunkSize)
    int firstChunkSize() const;
    /*!
     * Set the size of first chunk (number of groups to fetch) for asynchronous
     * and streamed queries.
     *
     * \param size First chunk size.
     */
    void setFirstChunkSize(int size);

    Q_PROPERTY(int limit READ limit WRITE setLimit)
    int limit() const;
    /*!
     * Set number of groups to fetch in the next query.
     *
     * \param limit Query limit.
     */
    void setLimit(int limit);

    Q_PROPERTY(int offset READ offset WRITE setOffset)
    int offset() const;
    /*!
     * Set offset for the next query.
     *
     * \param offset Query offset.
     */
    void setOffset(int offset);

    /*!
     * Convenience method for getting the group data without QVariant casts.
     *
     * \param index Model index.
     * \return group
     */
    GroupObject *group(int groupId) const;

    QList<GroupObject*> groups() const;

    /*!
     * Add a new group. If successful, group.id() is updated.
     *
     * \param group Group data to be inserted into the database.
     *
     * \return true if successful, otherwise false
     */
    bool addGroup(Group &group);

    /*!
     * Add new groups. If successful, group.id() is updated for added groups.
     *
     * \param groups Group data to be inserted into the database.
     *
     * \return true if successful, otherwise false
     */
    bool addGroups(QList<Group> &groups);

    /*!
     * Modifies a group. This will update a group with a matching id in
     * the database.
     * group.lastModified() is automatically updated.
     * \param group Group to be modified.
     * \return true if successful, otherwise false
     */
    bool modifyGroup(Group &group);

    /*!
     * Reset model to groups with the specified accounts and contacts
     * (if any). Groups with no messages are not included in the
     * initial results, but empty groups created elsewhere will appear
     * in the model (TODO: how should this behave?).
     *
     * \param localUid Local account (/org/freedesktop/Telepathy/Account/...).
     * \param remoteUid Remote contact uid (example: user@gmail.com).
     * \return true if successful, otherwise false
     */
    bool getGroups(const QString &localUid = QString(),
                   const QString &remoteUid = QString());

    /*!
     * Delete groups from database.
     *
     * \param groupIds List of group ids to be deleted.
     * \param deleteMessages If true (default), also delete all messages in the groups.
     * \return true if successful, otherwise false
     */
    bool deleteGroups(const QList<int> &groupIds, bool deleteMessages = true);

    /*!
     * Delete all groups from database.
     *
     * \return true if successful, otherwise false
     */
    bool deleteAll();

    /*!
     * Mark all unread events in group as read.
     * NOTE: this method will not cause event model update,
     * only groupsUpdated signal is emitted.
     *
     * \param id Database id of the group.
     * \return true if successful, otherwise false
     */
    bool markAsReadGroup(int id);

    /*!
     * Update groups data
     *
     * \param groups
     */
    void updateGroups(QList<Group> &groups);

    Q_PROPERTY(bool isReady READ isReady NOTIFY modelReady)
    bool isReady() const;

    /*!
     * Provide background thread for running database queries and blocking operations.
     * It allows to avoid blocking when the model used in the main GUI thread.
     * This function will cancel any outgoing requests. If thread is NULL,
     * model's thread is used for quereis.
     *
     * The thread should be started before making any queries and it should not
     * be terminated before deleting the model.
     *
     * \param thread running thread
    **/
    void setBackgroundThread(QThread *thread);
    QThread* backgroundThread();

    /*!
     * Return an instance of TrackerIO that can be used for low-level queries.
     * \return a TrackerIO
     */
    TrackerIO& trackerIO();

    /*!
     * If enabled (default), Group::contactId and Group::contactName in model
     * contents will be updated live (emitting dataChanged()) when
     * contacts are added, changed or deleted.
     * NOTE: This method must be called before getGroups() or it will
     * not have any effect.
     *
     * \param enabled If true, track contact changes.
     */
    void enableContactChanges(bool enabled);

    bool canFetchMore() const;
    void fetchMore();

Q_SIGNALS:
    /*!
     * Emitted when an async query is finished and the model has been filled.
     *
     * \param successful or false in case of an error
     *
     */
    void modelReady(bool successful);

    /*!
     * Emitted when group operation finishes.
     *
     * \param group ids
     * \param successful or false in case of an error
     */
    void groupsCommitted(const QList<int> &groupIds, bool successful);

    void groupAdded(GroupObject *group);
    void groupUpdated(GroupObject *group);
    void groupDeleted(GroupObject *group);

private:
    friend class GroupManagerPrivate;
    GroupManagerPrivate *d;
};

}

#endif
