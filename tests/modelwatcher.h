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

#ifndef COMMHISTORYTEST_MODELWATCHER_H
#define COMMHISTORYTEST_MODELWATCHER_H

#include "eventmodel.h"
#include "event.h"
#include "common.h"
#include <QList>

class ModelWatcher : public QObject
{
    Q_OBJECT

public:
    ModelWatcher(QObject *parent = 0);
    ~ModelWatcher();

    void setModel(CommHistory::EventModel *model);

    bool isFinished() const;
    void reset();

    // -1 for minCommitted = don't care (for example status messages)
    bool waitForCommitted(int count = 1);
    bool waitForAdded(int count = 1, int committed = -1);
    bool waitForUpdated(int count = 1);
    bool waitForDeleted(int count = 1);
    bool waitForModelReady();
    //void waitForSignals(int minCommitted = 0, int minAdded = 0, int minDeleted = 0);

#if 0
    int addedCount() { return m_addedCount; }
    int updatedCount() { return m_updatedCount; }
    int deletedCount() { return m_deletedCount; }
    int committedCount() { return m_committedCount; }
    QList<CommHistory::Event> lastAdded() { return m_lastAdded; }
    QList<CommHistory::Event> lastUpdated() { return m_lastUpdated; }
    int lastDeletedId() { return m_lastDeleted; }
    bool lastSuccess() {return m_success;}
#endif

public Q_SLOTS:
    void eventsAddedSlot(const QList<CommHistory::Event> &events);
    void eventsUpdatedSlot(const QList<CommHistory::Event> &events);
    void eventDeletedSlot(int eventId);
    void eventsCommittedSlot(const QList<CommHistory::Event> &events, bool successful);
    void modelReadySlot(bool success);

public:
    static int m_watcherId;
    bool m_signalsConnected;
    CommHistory::EventModel *m_model;
    int m_minCommitCount;
    int m_minAddCount;
    int m_committedCount;
    int m_minDeleteCount;
    int m_addedCount;
    int m_updatedCount;
    int m_deletedCount;
    QList<CommHistory::Event> m_lastAdded;
    QList<CommHistory::Event> m_lastUpdated;
    int m_lastDeleted;
    bool m_eventsCommitted;
    bool m_dbusSignalReceived;
    bool m_modelReady;
    bool m_success;
};

#endif
