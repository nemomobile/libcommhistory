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

#ifndef COMMHISTORY_CLASSZEROSMSMODEL_H
#define COMMHISTORY_CLASSZEROSMSMODEL_H

#include "eventmodel.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class ClassZeroSMSModelPrivate;

/*!
 * \class ClassZeroSMSModel
 * \brief In-memory model containing class 0 SMS events
 */
class LIBCOMMHISTORY_EXPORT ClassZeroSMSModel : public EventModel
{
    Q_OBJECT

public:
    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    ClassZeroSMSModel(QObject *parent = 0);

    /*!
     * reimp EventModel::deleteEvent()
     */
    bool deleteEvent(int id);

private:
    Q_DECLARE_PRIVATE(ClassZeroSMSModel);

};

} // namespace

#endif // CLASSZEROSMSMODEL_H
