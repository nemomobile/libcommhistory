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

#ifndef CALLMODELTEST_H
#define CALLMODELTEST_H

#include <QObject>
#include <QList>
#include "callevent.h"
#include "callmodel.h"

using namespace CommHistory;

class TestCallItem;

class CallModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testAddEvent();
    void testDeleteEvent();
    void testGetEventsTimeTypeFilter_data();
    void testGetEventsTimeTypeFilter();
    void testSortByContactUpdate();
    void testSortByTimeUpdate();
    void testSIPAddress();
    void testLimit();
    void deleteAllCalls();
    void testMarkAllRead();
    void testModifyEvent();
    void testMinimizedPhone();
    void cleanupTestCase();

private:
    void testGetEvents( CallModel::Sorting sorting, int rowCount, QList<TestCallItem> calls );
};

#endif
