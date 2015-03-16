/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Slava Monich <slava.monich@jolla.com>
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

#ifndef COMMHISTORY_MMSREADREPORTMODEL_H
#define COMMHISTORY_MMSREADREPORTMODEL_H

#include "eventmodel.h"

namespace CommHistory {

/**
 * Model for accessing events that require read reports for them to be sent.
 * Initialize with getEvent() or getEvents().
 */
class LIBCOMMHISTORY_EXPORT MmsReadReportModel: public EventModel
{
    Q_OBJECT

public:
    MmsReadReportModel(QObject *parent = 0);
    ~MmsReadReportModel();

    /**
     * Returns number of events in the model
     */
    int count() const;

    /**
     * Resets model to the specified event. If this event does not require
     * read report for it to be sent, returns false even if the event with
     * this id exists in the database.
     */
    bool getEvent(int eventId);

    /**
     * Resets model to events from the specified group.
     */
    bool getEvents(int groupId);

    /**
     * Checks if an external event would have been accepted by the model.
     */
    static bool acceptsEvent(const Event &event);

private:
    class Private;
    Private* d;
};

}

#endif // COMMHISTORY_MMSREADREPORTMODEL_H
