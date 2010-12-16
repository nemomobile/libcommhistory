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

#ifndef EVENTMODELTEST_H
#define EVENTMODELTEST_H

#include <QObject>
#include "event.h"

class EventModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testAddEvent();
    void testAddEvents();
    void testModifyEvent();
    void testDeleteEvent();
    void testDeleteEventGroupUpdated();
    void testMessageToken();
    void testVCard();
    void testDeliveryStatus();
    void testFindEvent();
    void testReportDelivery();
    void testMessageParts();
    void testCcBcc();
    void testStreaming_data();
    void testStreaming();
    void testModifyInGroup();
    void testMessagePartsQuery_data();
    void testMessagePartsQuery();
    void cleanupTestCase();

    void groupsUpdatedSlot(const QList<int> &groupIds);
    void groupsDeletedSlot(const QList<int> &groupIds);
};

#endif
