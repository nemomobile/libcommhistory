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

#ifndef COMMHISTORY_CALLMODEL_H
#define COMMHISTORY_CALLMODEL_H

#include "eventmodel.h"
#include "event.h"
#include "callevent.h"
#include "group.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class CallModelPrivate;

/*!
 * \class CallModel
 *
 * \brief Model for accessing the call history. Initialize with getEvents().
 *
 * CallModel is a model class to access call events. It uses a
 * grouped tree mode by default. Flat mode can be used to get all the call
 *  events based on type and time filters. Call events are grouped together
 * if they have the same call type (dialed, received, missed), they were initiated
 * to/from the same remote party and directly follow eachother in the timeline.
 * Furthermore sorting applies on the events: by time or by type. Sorting is set
 * in the constructor, and can be modified later in the getEvents() method as
 * parameter. If no sorting is specified, by contact is used as default.
 *
 * Tracking contact changes is enabled by default; call
 * enableContactChanges(false) after creating the model to disable.
 *
 */
class LIBCOMMHISTORY_EXPORT CallModel: public EventModel
{
    Q_OBJECT
    Q_ENUMS( Sorting )

public:
    enum Sorting
    {
        SortByContact = 0,
        SortByTime,
        SortByType,
        SortByService
    };

public:
    /*!
     * \brief Model constructor.
     *
     * \param parent Parent object.
     */
    CallModel(QObject *parent = 0);

    /*!
     * \brief Model constructor.
     * \deprecated DO NOT use this method, it is deprecated. You should use CallModel( QObject* ) and setFilter(CallModel::Sorting, CallEvent::CallType, const QDateTime &) instead.
     *
     * \param sortBy Sorting of call events.
     * \param parent Parent object.
     */
    CallModel(CallModel::Sorting sorting, QObject* parent);

    /*!
     * Destructor.
     */
    ~CallModel();

    /*!
     * \brief Sets optional filters.
     *
     * Sets optional filters. It will result in a new tracker query if  called
     * after getEvents().
     *
     * \param sortBy Sets sorting of call events in the result set.
     * \param type Only specified type events appear in result set, if other
     * than CallEvent::UnknownCallType.
     * \param referenceTime Only call events with newer startTime() are in
     * result set if value is valid.
     *
     * \return true if successful; false, otherwise
     */
    bool setFilter(CallModel::Sorting sortBy = SortByContact,
                   CallEvent::CallType type = CallEvent::UnknownCallType,
                   const QDateTime &referenceTime = QDateTime());

    /*!
     * \brief Resets model and fetch call events.
     *
     * \return true if successful; false, otherwise.
     */
    bool getEvents();

    /*!
     * \brief Sets optional filters.
     * \deprecated DO NOT use this method, it is deprecated. You should use getEvents() and/or setFilter(CallModel::Sorting, CallEvent::CallType, const QDateTime &) instead.
     *
     * Sets optional filters. It will result in a new tracker query if  called
     * after getEvents().
     *
     * \param sortBy Sets sorting of call events in the result set.
     * \param type Only specified type events appear in result set, if other
     * than CallEvent::UnknownCallType.
     * \param referenceTime Only call events with newer startTime() are in
     * result set if value is valid.
     *
     * \return true if successful; false, otherwise
     */
    bool getEvents(CallModel::Sorting sortBy,
                   CallEvent::CallType type = CallEvent::UnknownCallType,
                   const QDateTime &referenceTime = QDateTime());

    /*!
     * \brief Deletes all call events from tracker and clears model.
     *
     * \return true if successful; false, otherwise.
     */
    bool deleteAll();

    /*!
     * \brief Marks all call events as read.
     *
     * \return true if successful; false otherwise.
     */
    bool markAllRead();

    // reimp
    /* NOTE: With streamed queries, event counts might be incorrect at
     * chunk boundaries when sorting by time.
     */
    virtual void setQueryMode( EventModel::QueryMode mode );

    virtual bool addEvent( Event &event );

    virtual bool modifyEvent( Event &event );

    virtual bool deleteEvent( int id );

    virtual bool deleteEvent( Event &event );

private:
    Q_DECLARE_PRIVATE(CallModel);
};

}

#endif
