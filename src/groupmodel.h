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

#ifndef COMMHISTORY_GROUPMODEL_H
#define COMMHISTORY_GROUPMODEL_H

#include <QAbstractItemModel>

#include "eventmodel.h"
#include "group.h"
#include "groupobject.h"
#include "groupmanager.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class GroupModelPrivate;
class DatabaseIO;

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
class LIBCOMMHISTORY_EXPORT GroupModel: public QAbstractTableModel
{
    Q_OBJECT

    Q_ENUMS(ColumnId)

public:
    enum ColumnId {
        GroupId,
        LocalUid,
        RemoteUids,
        ChatName,       // not implemented
        EndTime,
        UnreadMessages,
        LastEventId,
        Contacts,       // TODO: deprecated
        LastMessageText,
        LastVCardFileName,
        LastVCardLabel,
        LastEventType,
        LastEventStatus,
        IsPermanent,
        LastModified,
        StartTime,
        NumberOfColumns
    };

    enum Role {
        GroupRole = Qt::UserRole,
        ContactIdsRole,
        GroupObjectRole,
        WeekdaySectionRole,
        BaseRole = Qt::UserRole + 1000
    };

    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    GroupModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~GroupModel();

    /*!
     * Group manager used for the list of groups. This may be shared with other models.
     * User is responsible for creating the manager, and querying groups with the desired
     * parameters.
     *
     * If not set, a manager will be created internally and settings copied to it.
     */
    Q_PROPERTY(QObject* manager READ manager WRITE setManager)
    GroupManager *manager() const;
    void setManager(GroupManager *manager);
    void setManager(QObject *manager) { setManager(qobject_cast<GroupManager*>(manager)); }

    /*!
     * Set query mode. See EventModel::setQueryMode().
     */
    void setQueryMode(EventModel::QueryMode mode);

    /*!
     * Set chunk size (number of groups to fetch) for asynchronous and
     * streamed queries.
     *
     * \param size Chunk size.
     */
    virtual void setChunkSize(uint size);

    /*!
     * Set the size of first chunk (number of groups to fetch) for asynchronous
     * and streamed queries.
     *
     * \param size First chunk size.
     */
    void setFirstChunkSize(uint size);

    /*!
     * Set number of groups to fetch in the next query.
     *
     * \param limit Query limit.
     */
    virtual void setLimit(int limit);

    /*!
     * Set offset for the next query.
     *
     * \param offset Query offset.
     */
    virtual void setOffset(int offset);

    /*!
     * Convenience method for getting the group data without QVariant casts.
     *
     * \param index Model index.
     * \return group
     */
    Group group(const QModelIndex &index) const;

    GroupObject *groupObject(const QModelIndex &index) const;
    Q_INVOKABLE CommHistory::GroupObject *at(int row) const { return groupObject(index(row, 0)); }

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
     * Update groups data, only model is updated
     *
     * \param groups
     */
    void updateGroups(QList<Group> &groups);

    bool isReady() const;
    EventModel::QueryMode queryMode() const;
    uint chunkSize() const;
    uint firstChunkSize() const;
    int limit() const;
    int offset() const;

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
     * If set to ResolveImmediately, contacts will be resolved for all groups, and changes
     * to contacts will be updated live (emitting dataChanged()). Contacts
     * will be resolved before groups are inserted into the model, and the
     * modelReady() signal indicates that all events are inserted and all
     * contacts are resolved.
     *
     * If set to ResolveOnDemand, contacts will be resolved only when
     * explicitly requested.  Changes to contacts will be reported.
     */
    void setResolveContacts(GroupManager::ContactResolveType type);

    /* reimp */
    virtual bool canFetchMore(const QModelIndex &parent) const;
    virtual void fetchMore(const QModelIndex &parent);
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    virtual QHash<int, QByteArray> roleNames() const;
    /***/

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

private:
    friend class GroupModelPrivate;
    GroupModelPrivate *d;
};

}

#endif
