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

#ifndef COMMHISTORY_GROUPMODEL_H
#define COMMHISTORY_GROUPMODEL_H

#include <QAbstractItemModel>
#include <QSqlError>

#include "eventmodel.h"
#include "group.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class GroupModelPrivate;
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
        TotalMessages,
        UnreadMessages,
        SentMessages,
        LastEventId,
        ContactId,
        ContactName,
        LastMessageText,
        LastVCardFileName,
        LastVCardLabel,
        LastEventType,
        LastEventStatus,
        IsPermanent,
        LastModified,
        NumberOfColumns
    };

    enum Role {
        GroupRole = Qt::UserRole,
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
     * Get details of the last error that occurred when using the model.
     *
     * \return error
     */
    QSqlError lastError() const;

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

    /*!
     * Add a new group. If successful, group.id() and group.isPermanent() are updated.
     *
     * \param group Group data to be inserted into the database.
     * \toModelOnly Indicates wheter or not to save instance to database. //TODO: remove on next API break
     * \return true if successful. Sets lastError() on failure.
     */
    bool addGroup(Group &group, bool toModelOnly = false);

    /*!
     * Modifies a group. This will update a group with a matching id in
     * the database.
     * group.lastModified() is automatically updated.
     * \param group Group to be modified.
     * \return true if successful. Sets lastError() on failure.
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
     * \return true if successful. Sets lastError() on failure.
     */
    bool getGroups(const QString &localUid = QString(),
                   const QString &remoteUid = QString());

    /*!
     * Delete groups from database.
     *
     * \param groupIds List of group ids to be deleted.
     * \param deleteMessages If true (default), also delete all messages in the groups.
     * \return true if successful
     */
    bool deleteGroups(const QList<int> &groupIds, bool deleteMessages = true);

    /*!
     * Delete all groups from database.
     *
     * \return true if successful
     */
    bool deleteAll();

    /*!
     * Mark all unread events in group as read.
     * NOTE: this method will not cause event model update,
     * only groupsUpdated signal is emitted.
     *
     * \param id Database id of the group.
     * \return true if successful
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

    /* reimp */
    virtual bool canFetchMore(const QModelIndex &parent) const;
    virtual void fetchMore(const QModelIndex &parent);
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    /***/

Q_SIGNALS:
    /*!
     * Emitted when an async query is finished and the model has been filled.
     */
    void modelReady();

private:
    friend class GroupModelPrivate;
    GroupModelPrivate *d;
};

}

#endif
