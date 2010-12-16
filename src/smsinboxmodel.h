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

#ifndef COMMHISTORY_SMSINBOXMODEL_H
#define COMMHISTORY_SMSINBOXMODEL_H

#include "eventmodel.h"
#include "event.h"
#include "callevent.h"
#include "group.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class SMSInboxModelPrivate;

/*!
 * \class SMSInboxModel
 * \deprecated Do not use this class.
 *
 * Model for accessing received SMS messages. Initialize with getEvents().
 */
class LIBCOMMHISTORY_EXPORT SMSInboxModel: public EventModel
{
    Q_OBJECT

public:
    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    SMSInboxModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~SMSInboxModel();

    /*!
     * Reset model and fetch draft events.
     *
     * \return true if successful, Sets lastError() on failure.
     */
    bool getEvents();

private:
    Q_DECLARE_PRIVATE(SMSInboxModel);
};

}

#endif
