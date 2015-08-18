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

#ifndef COMMHISTORY_CONTACTGROUPMODEL_H
#define COMMHISTORY_CONTACTGROUPMODEL_H

#include <QAbstractItemModel>

#include "libcommhistoryexport.h"
#include "contactgroup.h"
#include "groupmanager.h"

namespace CommHistory {

class GroupObject;
class ContactGroupModelPrivate;

/*!
 * \class ContactGroupModel
 *
 * This is experimental API, subject to change without notice or compatibility.
 *
 * ContactGroupModel builds on GroupManager by coalescing groups (conversations)
 * related to the same contact into a ContactGroup, and offering a sorted model
 * of those.
 *
 */
class LIBCOMMHISTORY_EXPORT ContactGroupModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_ENUMS(ColumnId)

public:
    enum ColumnId {
        ContactIds,
        ContactNames, // TODO: Obsolete
        EndTime,
        UnreadMessages,
        LastEventGroup,
        LastEventId,
        LastMessageText,
        LastVCardFileName,
        LastVCardLabel,
        LastEventType,
        LastEventStatus,
        LastEventIsDraft,
        LastModified,
        StartTime,
        Groups,
        DisplayNames,
        NumberOfColumns
    };

    enum {
        ContactGroupRole = Qt::UserRole,
        WeekdaySectionRole,
        BaseRole = Qt::UserRole + 2000
    };

    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    ContactGroupModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~ContactGroupModel();

    /*!
     * Group manager used for the list of groups. This may be shared with other models.
     * User is responsible for creating the manager, and querying groups with the desired
     * parameters.
     */
    Q_PROPERTY(QObject* manager READ manager WRITE setManager NOTIFY managerChanged)
    GroupManager *manager() const;
    void setManager(GroupManager *manager);
    void setManager(QObject *manager) { setManager(qobject_cast<GroupManager*>(manager)); }

    /*!
     * Convenience method to get the ContactGroup by row without QVariant casts
     *
     * \param index Model index.
     * \return group
     */
    ContactGroup *at(const QModelIndex &index) const;
    Q_INVOKABLE QObject *at(int row) const { return at(index(row, 0)); }

    /*!
     * List of contact groups in the model
     */
    Q_PROPERTY(QList<QObject*> contactGroups READ contactGroups)
    QList<QObject*> contactGroups() const;

    /* reimp */
    virtual bool canFetchMore(const QModelIndex &parent) const;
    virtual void fetchMore(const QModelIndex &parent);
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual QHash<int,QByteArray> roleNames() const;
    /***/

Q_SIGNALS:
    /*!
     * Emitted when an async query is finished and the model has been filled.
     *
     * \param successful or false in case of an error
     *
     */
    void modelReady(bool successful);
    void managerChanged();

    void contactGroupCreated(CommHistory::ContactGroup *group);
    void contactGroupChanged(CommHistory::ContactGroup *group);
    void contactGroupRemoved(CommHistory::ContactGroup *group);

private:
    friend class ContactGroupModelPrivate;
    ContactGroupModelPrivate *d;
};

}

#endif
