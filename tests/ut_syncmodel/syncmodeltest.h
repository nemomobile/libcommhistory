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
#include <QStringList>
#include <QList>
#include "event.h"
#include "syncsmsmodel.h"

using namespace CommHistory;

class SyncModelTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void initTestCase();
    void addSmsEvents_data();
    void addSmsEvents();
    void addModifyGetSingleSmsEvents();
    void readAddedSmsEventsFromConvModel();
    void cleanupTestCase();

private:
    bool addEvent(int parentId, int groupId, const QDateTime& sentReceivedTime,
                  const QString& localId, const QString& remoteId,
                  const QString& text, bool read);
    bool  modifyEvent( int itemId, int parentId, int groupId, const QDateTime &lastModTime,
                                     const QString& localId, const QString& remoteId, const QString& text, bool read);
    void evaluateModel(const SyncSMSModel &model, const QStringList &expectedListMsgs);
    int numAddedEvents;
    int itemId;

};

#endif
