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

#ifndef COMMHISTORY_UNREADEVENTSMODEL_H
#define COMMHISTORY_UNREADEVENTSMODEL_H

#include "eventmodel.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class UnreadEventsModelPrivate;

/*!
 * \class UnreadEventsModel
 * \deprecated Do not use this class.
 * \brief Model representing unread events, grouped by contact or remote id
 * e.g. phone number or IM user id
 * todo: currently model is flat, doesnt group events by contacts
 */
class LIBCOMMHISTORY_EXPORT UnreadEventsModel : public EventModel
{
    Q_OBJECT

public:
    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    explicit UnreadEventsModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~UnreadEventsModel();

    // reimp: EventModel::addEvent
    bool addEvent(Event &event);

    /*!
     * Reset model and fetch unread events.
     * \param bool, by default, only incoming events are fetched
     * \return true if successful, otherwise false
     */
    bool getEvents(bool includeSentEvents = false);

private:
    Q_DECLARE_PRIVATE(UnreadEventsModel);

};

} // namespace CommHistory

#endif // UNREADEVENTSMODEL_H
