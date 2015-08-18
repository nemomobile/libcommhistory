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

#ifndef COMMHISTORY_CONVERSATIONMODEL_H
#define COMMHISTORY_CONVERSATIONMODEL_H

#include "eventmodel.h"
#include "group.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class ConversationModelPrivate;

/*!
 * \class ConversationModel
 *
 * Model for accessing conversations. Initialize with
 * getEvents(). Use setFilter() to filter the visible messages.
 *
 * To resolve contacts for events, call setResolveContacts(true) before getEvents.
 * Contact changes will also be tracked and updated in the model. 
 *
 * Tree mode groups messages by date. Parent indexes have invalid
 * events with the texts "Today", "Yesterday" "Last week", "Last month"
 * and "Older".
 */
class LIBCOMMHISTORY_EXPORT ConversationModel: public EventModel
{
    Q_OBJECT

public:
    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    ConversationModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~ConversationModel();

    /*!
     * Set optional filter for conversation. Will result in a new
     * tracker query if called after getEvents().
     * Account filtering is useless at the moment with service-specific
     * conversations; left in for future compatibility.
     *
     * \param type Event type (IM or SMS).
     * \param account Local telepathy account path.
     * \param direction Event direction.
     * \return true if successful, otherwise false
     */
    bool setFilter(Event::EventType type = Event::UnknownType,
                   const QString &account = QString(),
                   Event::EventDirection direction = Event::UnknownDirection);

    /*!
     * Reset model to events from the specified group.
     *
     * \param groupId Valid group id
     *
     * \return true if successful, otherwise false
     */
    bool getEvents(int groupId);
    /*!
     * Reset model to events from the specified groups.
     *
     * \param groupIds List of valid group IDs
     * \return true if successful, otherwise false
     */
    bool getEvents(QList<int> groupIds);
    /*!
     * Reset model to all events.
     *
     * \return true if successful, otherwise false
     */
    bool getEvents();

    virtual bool canFetchMore(const QModelIndex &parent) const;
    virtual void fetchMore(const QModelIndex &parent);

private:
    Q_DECLARE_PRIVATE(ConversationModel)
};

}

#endif
