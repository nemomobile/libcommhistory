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
#include "queryresult.h"

namespace CommHistory {

class TrackerIO;

/*!
 * \class QueryRunner
 *
 * Helper thread for async tracker queries.
 */
class QueryRunner: public QObject
{
    Q_OBJECT

public:
    QueryRunner(TrackerIO *trackerIO, QObject *parent = 0);
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

    void runEventsQuery(const QString &query, const QList<Event::Property> &properties);

    void runGroupQuery(const QString &query);
    void runGroupedCallQuery(const QString &query);
    void runMessagePartQuery(const QString &query);

    void startQueue();

    void fetchMore();

Q_SIGNALS:
    void eventsReceived(int start, int end, QList<CommHistory::Event> events);
    void groupsReceived(int start, int end, QList<CommHistory::Group> groups);
    void messagePartsReceived(int eventId, QList<CommHistory::MessagePart> parts);
    void canFetchMoreChanged(bool canFetch);
    void modelUpdated(bool successful);

private Q_SLOTS:
    void dataReady(int totalCount);
    void finished();
    void nextSlot();
    void fetchMoreSlot();

private:
    void checkCanFetchMoreChange();
    void startNextQueryIfReady();
    bool reallyFetchMore(int pos);
    void readData();
    void endActiveQuery();

private:
    bool m_streamedMode;
    int m_chunkSize;
    int m_firstChunkSize;
    bool m_enableQueue;
    bool m_canFetchMore;

    QMutex m_mutex; // protects m_queries

    QList<CommHistory::QueryResult> m_queries;
    CommHistory::QueryResult m_activeQuery;
    int lastReadPos;

    TrackerIO *m_pTracker;

#ifdef DEBUG
    QTime m_timer;
#endif
};

}

#endif
