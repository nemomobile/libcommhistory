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
class DatabaseIO;

/*!
 * \class GroupManager
 *
 * Manager for querying and updating event groups (conversations) from
 * the database and in response to realtime changes.
 *
 * Call getGroups() to fill from the database. Changes made by other
 * processes will be automatically reflected in the manager.
 *
 * Use groupAdded, groupUpdated, and groupRemoved signals to monitor
 * changes, or the indiviual change signals for a GroupObject.
 */
class LIBCOMMHISTORY_EXPORT GroupManager : public QObject
{
    Q_OBJECT

public:
    GroupManager(QObject *parent = 0);
    virtual ~GroupManager();

    /*!
     * Set query mode. See EventModel::setQueryMode().
     */
    Q_PROPERTY(CommHistory::EventModel::QueryMode queryMode READ queryMode WRITE setQueryMode)
    EventModel::QueryMode queryMode() const;
    void setQueryMode(EventModel::QueryMode mode);

    /*!
     * Set chunk size (number of groups to fetch per request) for asynchronous
     * and streamed queries.
     */
    Q_PROPERTY(int chunkSize READ chunkSize WRITE setChunkSize)
    int chunkSize() const;
    void setChunkSize(int size);

    /*!
     * Set the size of first chunk for asynchronous and streamed queries.
     */
    Q_PROPERTY(int firstChunkSize READ firstChunkSize WRITE setFirstChunkSize)
    int firstChunkSize() const;
    void setFirstChunkSize(int size);

    /*!
     * Set number of groups to fetch in the next query.
     */
    Q_PROPERTY(int limit READ limit WRITE setLimit)
    int limit() const;
    void setLimit(int limit);

    /*!
     * Set offset for the next query, usually used with limit
     */
    Q_PROPERTY(int offset READ offset WRITE setOffset)
    int offset() const;
    void setOffset(int offset);

    /*!
     * Get the group object representing a group by ID
     *
     * \param groupId group ID
     * \return group
     */
    Q_INVOKABLE CommHistory::GroupObject *group(int groupId) const;

    /*!
     * Get the group object representing a local and remote UID pair
     *
     * The group must be already loaded in the GroupManager.
     *
     * \param localUid local account UID
     * \param remoteUid remote UID
     * \return group
     */
    Q_INVOKABLE CommHistory::GroupObject *findGroup(const QString &localUid, const QString &remoteUid) const;
    Q_INVOKABLE CommHistory::GroupObject *findGroup(const QString &localUid, const QStringList &remoteUids) const;

    /*!
     * Get a list of all loaded group objects
     */
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
     * \return true if successful, otherwise false
     */
    bool deleteGroups(const QList<int> &groupIds);

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

    /*!
     * True when data is loaded from the database
     */
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
     * Return an instance of DatabaseIO that can be used for low-level queries.
     * \return a DatabaseIO
     */
    DatabaseIO& databaseIO();

    /*!
     * If enabled, contacts will be resolved for all groups, and changes
     * to contacts will be updated live (emitting dataChanged()). Contacts
     * will be resolved before groups are inserted into the model, and the
     * modelReady() signal indicates that all events are inserted and all
     * contacts are resolved.
     *
     * \param enabled If true, resolve and update contacts for events
     */
    Q_PROPERTY(bool resolveContacts READ resolveContacts WRITE setResolveContacts NOTIFY resolveContactsChanged)
    void setResolveContacts(bool enabled);
    bool resolveContacts() const;

    // Deprecated name for setResolveContacts
    void enableContactChanges(bool enabled) { setResolveContacts(enabled); }

    bool canFetchMore() const;
    void fetchMore();

    void resolve(GroupObject &group);

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

    /*!
     * Emitted when a group is added to the manager for any reason, including
     * query results, activity in other processes, or addGroup().
     *
     * \param group New group object
     */
    void groupAdded(GroupObject *group);
    /*!
     * Emitted when the data of a group changes for any reason. GroupObject has
     * signals for monitoring specific changes.
     *
     * \param group Group
     */
    void groupUpdated(GroupObject *group);
    /*!
     * Emitted when a group is removed from the model for any reason. This does
     * not mean that the group is removed from the underlying database.
     *
     * \param group Group
     */
    void groupDeleted(GroupObject *group);
    /*!
     * Emitted when contact resolution policy is changed.
     */
    void resolveContactsChanged();

private:
    friend class GroupManagerPrivate;
    GroupManagerPrivate *d;
};

}

#endif
