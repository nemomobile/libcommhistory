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

#ifndef COMMHISTORY_OUTBOXMODEL_H
#define COMMHISTORY_OUTBOXMODEL_H

#include "eventmodel.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class OutboxModelPrivate;

/*!
 * \class OutboxModel
 * \deprecated Do not use this class.
 *
 * Model for accessing a single conversation. Initialize with
 * getEvents(). Use setFilter() to filter the visible messages.
 *
 * (not implemented yet) Tree mode groups messages by delivery status.
 */
class LIBCOMMHISTORY_EXPORT OutboxModel: public EventModel
{
    Q_OBJECT

public:
    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    OutboxModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~OutboxModel();

    /*!
     * Reset model and fetch draft events.
     *
     * \return true if successful, otherwise false
     */
    bool getEvents();

private:
    Q_DECLARE_PRIVATE(OutboxModel);
};

}

#endif
