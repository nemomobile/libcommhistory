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

#ifndef COMMHISTORY_GROUPMODEL_P_H
#define COMMHISTORY_GROUPMODEL_P_H

#include <QAbstractItemModel>
#include <QSqlError>
#include <QList>
#include <QPair>
#include <QtTracker/Tracker>

#include "groupmodel.h"
#include "eventmodel.h"
#include "group.h"

namespace CommHistory {

class QueryRunner;
class TrackerIO;
class ContactListener;
class CommittingTransaction;

class GroupModelPrivate: public QObject
{
    Q_OBJECT

    Q_DECLARE_PUBLIC(GroupModel);

public:
    // For async query safety - queue changes until the model is ready.
    typedef enum {
        GROUP_ADD,
        GROUP_MODIFY,
        GROUP_DELETE
    } GroupChangeType;

    GroupModel *q_ptr;

    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    GroupModelPrivate(GroupModel *parent = 0);

    /*!
     * Destructor.
     */
    ~GroupModelPrivate();

    QString newObjectPath();

    void addToModel(Group &group);
    void modifyInModel(Group &group, bool query = true);
    void deleteFromModel(Group &group);

    bool canFetchMore() const;

    void executeQuery(SopranoLive::RDFSelect &query);

    CommittingTransaction* commitTransaction(QList<Group> groups);

    void resetQueryRunner();
    void deleteQueryRunner();

    TrackerIO* tracker();
    void startContactListening();

public Q_SLOTS:
    void eventsAddedSlot(const QList<CommHistory::Event> &events);

    void groupAddedSlot(CommHistory::Group group);

    void groupsUpdatedSlot(const QList<int> &groupIds);
    void groupsUpdatedFullSlot(const QList<CommHistory::Group> &groups);

    void groupsDeletedSlot(const QList<int> &groupIds);

    void groupsReceivedSlot(int start, int end, QList<CommHistory::Group> result);

    void modelUpdatedSlot();

    void canFetchMoreChangedSlot(bool canFetch);

    void slotContactUpdated(quint32 localId,
                            const QString &contactName,
                            const QList< QPair<QString,QString> > &contactAddresses);

    void slotContactRemoved(quint32 localId);

Q_SIGNALS:
    void groupAdded(CommHistory::Group group);

    void groupsUpdated(const QList<int> &groupIds);
    void groupsUpdatedFull(const QList<CommHistory::Group> &groups);

    void groupsDeleted(const QList<int> &groupIds);

public:
    static uint modelSerial;

    EventModel::QueryMode queryMode;
    uint chunkSize;
    uint firstChunkSize;
    int queryLimit;
    int queryOffset;
    bool isReady;
    QList<Group> groups;
    QSqlError lastError;

    QString filterLocalUid;
    QString filterRemoteUid;

    QueryRunner *queryRunner;
    bool threadCanFetchMore;

    QThread *bgThread;

    TrackerIO *m_pTracker;

    QSharedPointer<ContactListener> contactListener;
    bool contactChangesEnabled;
};

}

#endif
