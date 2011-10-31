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

#ifndef COMMHISTORY_SYNCSMSMODEL_P_H
#define COMMHISTORY_SYNCSMSMODEL_P_H

#include <QVariant>

#include "syncsmsmodel.h"
#include "eventmodel_p.h"

class CommittingTransaction;
class QSparqlResult;

namespace CommHistory
{

using namespace CommHistory;

class SyncSMSModelPrivate : public EventModelPrivate
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(SyncSMSModel);

public:
    SyncSMSModelPrivate(EventModel *model,
                        int _parentId,
                        QDateTime _dtTime,
                        bool _lastModified);

    bool acceptsEvent(const Event &event) const;

public Q_SLOTS:
    void checkTokens(CommittingTransaction *transaction,
                     QSparqlResult *result,
                     QVariant arg);

public:
    QDateTime dtTime;
    int parentId;
    bool lastModified;
};

}

#endif
