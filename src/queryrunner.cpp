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

#include <QMutex>
#include <QtTracker/Tracker>
#include <QDebug>

#include "queryrunner.h"
#include "queryresult.h"
#include "event.h"
#include "messagepart.h"

using namespace SopranoLive;

using namespace CommHistory;

QueryRunner::QueryRunner(QObject *parent)
        : QObject(parent)
        , m_streamedMode(false)
        , m_chunkSize(0)
        , m_firstChunkSize(0)
        , m_enableQueue(false)
        , m_ready(true)
        , m_canFetchMore(false)
{
    qDebug() << __PRETTY_FUNCTION__;

    m_activeQuery.isValid = false;
}

QueryRunner::~QueryRunner()
{
    qDebug() << __PRETTY_FUNCTION__ << this << this->thread();

    if (m_activeQuery.isValid && m_activeQuery.model) {
        m_activeQuery.model.model()->disconnect(this);
        m_activeQuery.model->stopOperations();
        m_activeQuery.model->deleteLater();
    }
}

void QueryRunner::setStreamedMode(bool mode)
{
    m_streamedMode = mode;
}

void QueryRunner::setChunkSize(int size)
{
    m_chunkSize = size;
}

void QueryRunner::setFirstChunkSize(int size)
{
    m_firstChunkSize = size;
}

void QueryRunner::enableQueue(bool enable)
{
    m_enableQueue = enable;
}

void QueryRunner::runQuery(RDFSelect &query, QueryType queryType,
                           const Event::PropertySet &propertyMask)
{
    qDebug() << Q_FUNC_INFO << QThread::currentThread()  << this << "->";

    QMutexLocker locker(&m_mutex);

    QueryResult result;
    result.query = query;
    result.queryType = queryType;
    result.propertyMask = propertyMask;
    result.eventId = 0;
    m_queries.append(result);

#ifdef DEBUG
    m_timer.start();
#endif

    if (!m_enableQueue)
        QMetaObject::invokeMethod(this, "nextSlot", Qt::QueuedConnection);

    qDebug() << Q_FUNC_INFO << QThread::currentThread()  << this << "<-";
}

void QueryRunner::startQueue()
{
    QMetaObject::invokeMethod(this, "nextSlot", Qt::QueuedConnection);
}

void QueryRunner::fetchMore()
{
    qDebug() << Q_FUNC_INFO << QThread::currentThread() << this;

    QMetaObject::invokeMethod(this, "fetchMoreSlot", Qt::QueuedConnection);
}

void QueryRunner::startNextQueryIfReady()
{
    if (m_enableQueue && !m_ready)
        return;  // ongoing query and queue mode enabled

    // queue mode not enabled, stop active query
    if (m_activeQuery.isValid && m_activeQuery.model) {
        m_activeQuery.model.model()->disconnect(this);
        m_activeQuery.model->stopOperations();
        m_activeQuery.model->deleteLater();
        m_activeQuery.isValid = false;
    }

    m_mutex.lock();
    if (!m_queries.isEmpty()) {
        // start new query
        m_ready = false;
        m_activeQuery = m_queries.takeFirst();
        m_activeQuery.isValid = true;

        RDFStrategyFlags flags = RDFStrategy::DefaultStrategy;
        if (m_streamedMode) {
            flags = RDFStrategy::StreamingStrategy;
            // Set query limit to be the same as the first chunk size to stop automatic streaming
            // in Qt-Tracker after getting first chunk. We want to fetch rest of the stuff using fetchMore.
            m_activeQuery.query.limit(m_firstChunkSize > 0 ? m_firstChunkSize : m_chunkSize);
            ::tracker()->setServiceAttribute(QLatin1String("streaming_first_block_size"),
                QVariant(m_firstChunkSize > 0 ? m_firstChunkSize : m_chunkSize));
            ::tracker()->setServiceAttribute(QLatin1String("streaming_block_size"), QVariant(m_chunkSize));
        }

        m_activeQuery.model = ::tracker()->modelQuery(m_activeQuery.query, flags);
        connect(m_activeQuery.model.model(), SIGNAL(rowsInserted(const QModelIndex &, int, int)),
                this, SLOT(rowsInsertedSlot(const QModelIndex &, int, int)),
                Qt::DirectConnection);
        connect(m_activeQuery.model.model(), SIGNAL(modelUpdated()),
                this, SLOT(modelUpdatedSlot()),
                Qt::DirectConnection);

        m_activeQuery.columns.clear();
    }
    m_mutex.unlock();
}

void QueryRunner::nextSlot()
{
    qDebug() << Q_FUNC_INFO << QThread::currentThread() << this;

    startNextQueryIfReady();
}

void QueryRunner::fetchMoreSlot()
{
    qDebug() << Q_FUNC_INFO << QThread::currentThread();

    if (m_activeQuery.isValid && m_activeQuery.model) {
        // Need to reset first block size to normal block size here because of Qt-Tracker's fetchMore-behaviour:
        // fetchMore will otherwise use the original streaming_first_block_size and then we would
        // not get rest of the chunks as normal sized.
        // It would not be necessary to set model attribute here for every fetchMore call, but only for the
        // first one in the active query, but then you would have to check it from LiveNodes model every time if
        // it is already set and set it if needed.
        m_activeQuery.model->setModelAttribute("streaming_first_block_size", QVariant(m_chunkSize));
        m_activeQuery.model->fetchMore(QModelIndex());
    }
}

void QueryRunner::rowsInsertedSlot(const QModelIndex &parent,
                                   int start, int end)
{
    Q_UNUSED(parent);

    qDebug() << Q_FUNC_INFO << QThread::currentThread() << this;

    checkCanFetchMoreChange();

    // generate header mapping
    if (m_activeQuery.columns.isEmpty()) {
        for (int i = 0; i < m_activeQuery.model->columnCount(); i++) {
            m_activeQuery.columns.insert(
                m_activeQuery.model->headerData(i, Qt::Horizontal).toString(), i);
        }
    }

    if (m_activeQuery.queryType == EventQuery) {
        QList<Event> events;
        for (int row = start; row <= end; row++) {
            Event event;
            QueryResult::fillEventFromModel(m_activeQuery, row, event);
            events.append(event);
        }
        emit eventsReceived(start, end, events);
    } else if (m_activeQuery.queryType == GroupQuery) {
        QList<Group> groups;
        for (int row = start; row <= end; row++) {
            Group group;
            QueryResult::fillGroupFromModel(m_activeQuery, row, group);
            groups.append(group);
        }
        emit groupsReceived(start, end, groups);
    } else if (m_activeQuery.queryType == MessagePartQuery) {
        QList<MessagePart> parts;
        for (int row = start; row <= end; row++) {
            MessagePart part;
            QueryResult::fillMessagePartFromModel(m_activeQuery, row, part);
            parts.append(part);
        }
        emit messagePartsReceived(m_activeQuery.eventId, parts);
    }

#ifdef DEBUG
    qDebug() << "*** TIMER" << m_timer.elapsed();
#endif
}

void QueryRunner::checkCanFetchMoreChange()
{
    bool newFetchMore = m_activeQuery.model->canFetchMore(QModelIndex());
    if (newFetchMore != m_canFetchMore) {
        m_canFetchMore = newFetchMore;
        emit canFetchMoreChanged(m_canFetchMore);
    }
}

void QueryRunner::modelUpdatedSlot()
{
    qDebug() << Q_FUNC_INFO << QThread::currentThread() << this;

    bool continueNext = false;
    {
        QMutexLocker locker(&m_mutex);
        continueNext = m_enableQueue && !m_queries.isEmpty();
    }

    m_ready = true;

    if (continueNext) {
        // start next query from queue
        nextSlot();
    } else {
        /* We need to check the canFetchMore status of LiveNodes model also here, in addition to the check-up
           in event loop in run()-method, before emitting modelUpdated signal
           because if canFetchMore is FALSE and we do not indicate it here by emitting canFetchMoreChanged signal
           to the commhistory data model the client might get modelReady (commhistory data model emits modelReady
           as a result of catching modelUpdated, emitted here) before the commhistory data model knows that it cannot
           fetch more and then if the client asks canFetchMore it might still get true although it should be false. */
        checkCanFetchMoreChange();
        emit modelUpdated();
    }
}
