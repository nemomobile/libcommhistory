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

#ifndef DRAFTMODELTEST_H
#define DRAFTMODELTEST_H

#include <QObject>
#include <QList>
#include "event.h"

using namespace CommHistory;

class DraftModelTest : public QObject
{
    Q_OBJECT

public slots:
    void eventsAddedSlot(const QList<CommHistory::Event> &events);

private slots:
    void initTestCase();
    void getEvents();
    void addEvent();
    void cleanupTestCase();

private:
    int execLoop(int times);
};

#endif
