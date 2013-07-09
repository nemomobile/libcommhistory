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

#include <QtTest/QtTest>
#include <QDBusConnection>

#include "modelwatcher.h"

int ModelWatcher::m_watcherId = 0;

ModelWatcher::ModelWatcher(QEventLoop *loop, QObject *parent)
    : QObject(parent),
      m_signalsConnected(false),
      m_model(0),
      m_minCommitCount(0),
      m_minAddCount(0),
      m_committedCount(0),
      m_addedCount(0),
      m_updatedCount(0),
      m_deletedCount(0),
      m_lastDeleted(0),
      m_eventsCommitted(false),
      m_dbusSignalReceived(false),
      m_modelReady(false),
      m_loop(loop)
{
    if (!m_loop)
        m_loop = new QEventLoop(this);
}

ModelWatcher::~ModelWatcher()
{
}

void ModelWatcher::setLoop(QEventLoop *loop)
{
    m_loop = loop;
}

void ModelWatcher::setModel(CommHistory::EventModel *model)
{
    if (!m_signalsConnected) {
        QString objectPath = QString("/ModelTestWatcher%0").arg(m_watcherId++);
        QVERIFY(QDBusConnection::sessionBus().registerObject(objectPath, this));
        QDBusConnection::sessionBus().connect(
            QString(), QString(), "com.nokia.commhistory", "eventsAdded",
            this, SLOT(eventsAddedSlot(const QList<CommHistory::Event> &)));
        QDBusConnection::sessionBus().connect(
            QString(), QString(), "com.nokia.commhistory", "eventsUpdated",
            this, SLOT(eventsUpdatedSlot(const QList<CommHistory::Event> &)));
        QDBusConnection::sessionBus().connect(
            QString(), QString(), "com.nokia.commhistory", "eventDeleted",
            this, SLOT(eventDeletedSlot(int)));
        m_signalsConnected = true;
    }

    m_model = model;
    connect(m_model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)),
            this, SLOT(eventsCommittedSlot(const QList<CommHistory::Event>&, bool)));
    connect(m_model, SIGNAL(modelReady(bool)), this, SLOT(modelReadySlot(bool)));

    reset();
}

void ModelWatcher::waitForSignals(int minCommitted, int minAdded, int minDeleted)
{
    m_addedCount = 0;
    m_updatedCount = 0;
    m_deletedCount = 0;
    m_committedCount = 0;

    m_minCommitCount = minCommitted;
    m_minAddCount = minAdded;
    m_minDeleteCount = minDeleted;

    m_eventsCommitted = (m_minCommitCount == -1 ? true : false);
    m_dbusSignalReceived = false;

    m_loop->exec();
}

bool ModelWatcher::waitForModelReady(int msec)
{
    m_modelReady = false;
    QTime timer;
    timer.start();
    while (timer.elapsed() < msec && !m_modelReady)
        QCoreApplication::processEvents();

    return m_modelReady;
}

void ModelWatcher::eventsCommittedSlot(const QList<CommHistory::Event> &events,
                                       bool successful)
{
    qDebug() << Q_FUNC_INFO;

    m_committedCount += successful ? events.size() : 0;
    m_success = successful;

    if (!successful) {
        m_loop->exit(0);
        return;
    }

    if (!m_minCommitCount
        || (m_minCommitCount && m_committedCount >= m_minCommitCount)) {
        if (m_dbusSignalReceived)
            m_loop->exit(0);
        else
            m_eventsCommitted = true;
    }
}

void ModelWatcher::eventsAddedSlot(const QList<CommHistory::Event> &events)
{
    qDebug() << Q_FUNC_INFO;
    m_addedCount += events.count();
    m_lastAdded = events;

    if (!m_minAddCount
        || (m_minAddCount && m_addedCount >= m_minAddCount)) {
        if (m_eventsCommitted)
            m_loop->exit(0);
        else
            m_dbusSignalReceived = true;
    }
}

void ModelWatcher::eventsUpdatedSlot(const QList<CommHistory::Event> &events)
{
    qDebug() << Q_FUNC_INFO;
    m_updatedCount += events.count();
    m_lastUpdated = events;

    if (m_eventsCommitted)
        m_loop->exit(0);
    else
        m_dbusSignalReceived = true;
}

void ModelWatcher::eventDeletedSlot(int id)
{
    qDebug() << Q_FUNC_INFO;
    m_deletedCount++;
    m_lastDeleted = id;

    if (!m_minDeleteCount
        || (m_minDeleteCount && m_deletedCount >= m_minDeleteCount)) {
        if (m_eventsCommitted)
            m_loop->exit(0);
        else
            m_dbusSignalReceived = true;
    }
}

void ModelWatcher::modelReadySlot(bool success)
{
    qDebug() << Q_FUNC_INFO;
    m_modelReady = true;
    m_success = success;
    m_loop->exit(0);
}
