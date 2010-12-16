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

#ifndef COMMHISTORY_CALLMODEL_P_H
#define COMMHISTORY_CALLMODEL_P_H

#include <QList>

#include "callmodel.h"
#include "eventmodel.h"
#include "eventmodel_p.h"

namespace CommHistory
{

using namespace CommHistory;

class CallModelPrivate : public EventModelPrivate
{
    Q_OBJECT
    Q_DECLARE_PUBLIC( CallModel );

public:
    CallModelPrivate( EventModel *model );

    bool acceptsEvent( const Event &event ) const;

    int calculateEventCount( EventTreeItem *item );

    bool fillModel( int start, int end, QList<CommHistory::Event> events );

    bool belongToSameGroup( const Event &e1, const Event &e2 );

    void addToModel( Event &event );

    void eventsAddedSlot( const QList<Event> &events );

    void eventsUpdatedSlot( const QList<Event> &events );

    QModelIndex findEvent( int id ) const;

    void deleteFromModel( int id );

    /*!
     * Find real contact based on phone number or IM id.
     *
     * \param remoteId Phone number or IM id (for example, "user@host.com").
     * \param id UID for returned contact.
     * \param name Contact name or IM nickname, if any.
     * \return true if contact was found.
     */
    static bool resolveContact(const QString &remoteId,
                               int &contactId,
                               QString &contactName);

public Q_SLOTS:
    void slotEventsCommitted(const QList<CommHistory::Event> &events, bool success);

public:
    CallModel::Sorting sortBy;
    CallEvent::CallType eventType;
    QDateTime referenceTime;
    bool hasBeenFetched;
    bool deleteSync;

};

}

#endif
