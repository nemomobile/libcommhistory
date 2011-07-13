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

#ifndef COMMHISTORY_SYNCSMSMODEL_H
#define COMMHISTORY_SYNCSMSMODEL_H

#include <QDateTime>

#include "eventmodel.h"
#include "event.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

struct LIBCOMMHISTORY_EXPORT SyncSMSFilter
{
    int parentId;
    QDateTime time;
    bool lastModified;

    SyncSMSFilter(int _parentId = INBOX, QDateTime _time= QDateTime(), bool _lastModified = false)
        :parentId(_parentId),
        time(_time),
        lastModified(_lastModified)
    {
    }

};

class SyncSMSModelPrivate;
/*!
 * \class SyncSMSModel
 *  Model for syncing all the stored sms. Initialization of model is done with getEvents
 */
class LIBCOMMHISTORY_EXPORT SyncSMSModel : public EventModel
{
public:


    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    SyncSMSModel(int parentId = INBOX, QDateTime time= QDateTime(), bool lastModified = false, QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~SyncSMSModel();


    /*!
     * Reset model and fetch sms events. Messages are fetched based on SyncSMSFilter
     * This method is used to retrieve the sms present in device during sync session
     * \return true if successful, otherwise false
     */
    bool getEvents();

    /*!
      * if filter.parentId is set, then all messages whose parentId matches that of the filter would be fetched. If parentId is 'ALL', then all messages would be fetched, no constraint would be set in this case
      * If filter.time is set and lastModified and deleted are not set, then all messages whose sent/received time is greater or equal to filter.time would be fetched
      * If filter.time is set and either of lastModified or deleted are  set, then all messages whose sent/received time is lesser than  filter.time and lastModifiedTime is greater or equal to filter.time would be fetched
      */
    void setSyncSmsFilter(const SyncSMSFilter& filter);

private:
    Q_DECLARE_PRIVATE(SyncSMSModel);
};

}

#endif // SYNCSMSMODEL_H
