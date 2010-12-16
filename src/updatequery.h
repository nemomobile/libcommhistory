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

#ifndef COMMHISTORY_UPDATE_QUERY_H
#define COMMHISTORY_UPDATE_QUERY_H

#include <QtTracker/Tracker>

namespace CommHistory {

class UpdateQuery {
    typedef QPair<SopranoLive::RDFStatementList,SopranoLive::RDFStatementList> QueryOp;
public:
    void deletion(SopranoLive::RDFVariable const &subject,
                  SopranoLive::RDFVariable const &predicate,
                  SopranoLive::RDFVariable const &object = SopranoLive::RDFVariable()) {
        if (m_operations.isEmpty())
            startQuery();

        m_operations.top().first << SopranoLive::RDFStatement(subject, predicate, object);
    }

    void insertion(SopranoLive::RDFVariable const &subject,
                   SopranoLive::RDFVariable const &predicate,
                   SopranoLive::RDFVariable const &object,
                   bool modify = false) {
        if (m_operations.isEmpty())
            startQuery();

        if (modify) {
            // delete properties in separate queries
            startQuery();
            deletion(subject, predicate);
            endQuery();
        }

        m_operations.top().second << SopranoLive::RDFStatement(subject, predicate, object);
    }

    void startQuery() {
        m_operations.push(QueryOp());
    }

    void endQuery() {
        QueryOp op = m_operations.pop();

        if (!op.first.isEmpty())
            m_update.addDeletion(op.first);

        if (!op.second.isEmpty())
            m_update.addInsertion(op.second);
    }

    const SopranoLive::RDFUpdate& rdfUpdate() {
        while (!m_operations.isEmpty())
            endQuery();

        return m_update;
    }

private:
    SopranoLive::RDFUpdate m_update;
    QStack<QueryOp> m_operations;
};

}

#endif
