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

#ifndef GROUPMODELTEST_H
#define GROUPMODELTEST_H

#include <QObject>
#include <QList>
#include "event.h"
#include "group.h"

class QModelIndex;

using namespace CommHistory;

class GroupModelTest : public QObject
{
    Q_OBJECT

public slots:
    void eventsAddedSlot(const QList<CommHistory::Event> &);
    void groupAddedSlot(CommHistory::Group group);
    void groupsUpdatedSlot(const QList<int> &);
    void groupsUpdatedFullSlot(const QList<CommHistory::Group> &);
    void groupsDeletedSlot(const QList<int> &);
    void dataChangedSlot(const QModelIndex &, const QModelIndex &);

private slots:
    void initTestCase();
    void addGroups();
    void modifyGroup();
    void getGroups_data();
    void getGroups();
    void updateGroups();
    void deleteGroups();
    void streamingQuery_data();
    void streamingQuery();
    void deleteMmsContent();
    void cleanupTestCase();
    void init();
    void cleanup();

private:
    void idle(int msec);
};

#endif
