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

#ifndef EVENTSQUERY_H
#define EVENTSQUERY_H

#include "event.h"
#include "libcommhistoryexport.h"

#define COMMHISTORY_GRAPH_CALL_CHANNEL "commhistory:call-channels"
#define COMMHISTORY_GRAPH_MESSAGE_CHANNEL "commhistory:message-channels"

namespace CommHistory {

class EventsQueryPrivate;

class LIBCOMMHISTORY_EXPORT EventsQuery
{
public:
    /*!
     * \brief Construct a new query
     * \param propertySet properties to request
     */
    EventsQuery(const Event::PropertySet &propertySet);
    ~EventsQuery();

    /*!
     * \brief add a new pattern that will be appended to WHERE clause for the final query
     *
     * \return itself
     */
    EventsQuery& addPattern(const QString &pattern);

    /*!
     * \brief add a modifier that will be appended after the WHERE clause in the final query
     *
     * \param pattern with placeholders (%n) for variables
     *
     * \return itself
     */
    EventsQuery& addModifier(const QString &pattern);

    /*!
     * \brief replace %n with the lowest number with variable corresponding to property in
     * the latest added pattern or modifier
     *
     * \param pattern with placeholders (%n) for variables
     *
     * \return itself
     */
    EventsQuery& variable(Event::Property property);

    /*!
     * \brief Specify usage of DISTINCT with SELECT
     *
     * \param true if use DISTINCT, false otherwise
     */
    void setDistinct(bool distinct);

    /*!
     * \return true if use DISTINCT, false otherwise
     */
    bool distinct() const;


    /*!
     * \brief generate final query
     *
     * \return final SPARQL query
     */
    QString query() const;

    /*!
     * \brief event properties in the same order as columns in the result
     * The set could be equal to the one provided to ctor or extend it. ??
     * The result will be valid only if called after query().
     *
     * \return requested properties
     */
    QList<Event::Property> eventProperties() const;

    /*!
     * Add extra projection, available after event properties columns
     * in the added order.
     *
     * \param projection statment with placeholders (%n) for variables
     */
    EventsQuery& addProjection(const QString &projection);

private:
    friend class EventsQueryPrivate;
    EventsQueryPrivate * const d;
};

} //namespace

#endif // EVENTSQUERY_H
