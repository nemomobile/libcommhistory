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

#ifndef COMMHISTORY_QUERYTHREAD_H
#define COMMHISTORY_QUERYTHREAD_H

#include <QMutex>
#include <QtTracker/Tracker>

#include "event.h"
#include "group.h"
#include "trackerio.h"
#include "queryresult.h"

namespace CommHistory {

/*!
 * \class QueryRunner
 *
 * Helper thread for async tracker queries.
 */
class QueryRunner: public QObject
{
    Q_OBJECT

public:
    QueryRunner(QObject *parent = 0);
    ~QueryRunner();

    void setStreamedMode(bool mode);

    void setChunkSize(int size);

    void setFirstChunkSize(int size);

    // If false (default), runQuery() cancels any ongoing queries before
    // starting a new one.

    // If true, runQuery() will queue multiple queries and results will
    // be returned sequentially. The queue will not be started until you
    // call startQueue(). Note that queued queries are only started
    // after the previous query has been fully completed, so streamed
    // queries will block the queue until all available rows have been
    // fetched.
    void enableQueue(bool enable = true);

    void runQuery(SopranoLive::RDFSelect &query, QueryType queryType,
                  const Event::PropertySet &propertyMask = Event::allProperties());

    void startQueue();

    void fetchMore();

Q_SIGNALS:
    void eventsReceived(int start, int end, QList<CommHistory::Event> events);
    void groupsReceived(int start, int end, QList<CommHistory::Group> groups);
    void messagePartsReceived(int eventId, QList<CommHistory::MessagePart> parts);
    void canFetchMoreChanged(bool canFetch);
    void modelUpdated();

private Q_SLOTS:
    void rowsInsertedSlot(const QModelIndex &parent, int start, int end);
    void modelUpdatedSlot();
    void nextSlot();
    void fetchMoreSlot();

private:
    /*!
     * \brief Checks from Qt-Tracker's LiveNodes model if it can fetch more and compares that status
     * to the previous one. If status has been changed from previous check, then emits a canFetchMoreChanged
     * signal with the new status (boolean value).
     */
    void checkCanFetchMoreChange();
    void startNextQueryIfReady();

private:
    bool m_streamedMode;
    int m_chunkSize;
    int m_firstChunkSize;
    bool m_enableQueue;
    bool m_ready;
    bool m_canFetchMore;

    QMutex m_mutex; // protects m_queries

    QList<CommHistory::QueryResult> m_queries;
    CommHistory::QueryResult m_activeQuery;

#ifdef DEBUG
    QTime m_timer;
#endif
};

}

#endif
