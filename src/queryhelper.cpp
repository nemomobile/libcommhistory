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

#include <QObject>
#include <QSparqlConnection>
#include <QSparqlResult>
#include <QSparqlResultRow>
#include <QSparqlError>
#include <QThread>

#include "trackerio.h"
#include "trackerio_p.h"
#include "queryhelper.h"

using namespace CommHistory;

QueryHelper::QueryHelper(TrackerIO *tracker,
                         bool autodelete)
    : m_tracker(tracker),
      m_autodelete(autodelete)
{
}

void QueryHelper::runQuery(const QSparqlQuery &query,
                           QThread *backgroundThread)
{
    if (backgroundThread)
        moveToThread(backgroundThread);

    m_result = m_tracker->d->connection().exec(query);
    connect(m_result, SIGNAL(finished()),
            this, SLOT(queryFinished()));
}

void QueryHelper::queryFinished()
{
    emit finished(m_result);

    if (m_autodelete)
        deleteLater();
}
