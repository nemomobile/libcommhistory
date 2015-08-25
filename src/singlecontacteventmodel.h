/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
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

#ifndef COMMHISTORY_SINGLE_CONTACT_EVENT_MODEL_H
#define COMMHISTORY_SINGLE_CONTACT_EVENT_MODEL_H

#include "eventmodel.h"
#include "libcommhistoryexport.h"

#include <QContactId>

namespace CommHistory {

class SingleContactEventModelPrivate;

/*!
 * \class SingleContactEventModel
 * \brief Model representing events from a single contact
 */
class LIBCOMMHISTORY_EXPORT SingleContactEventModel : public EventModel
{
    Q_OBJECT

public:
    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    explicit SingleContactEventModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~SingleContactEventModel();

    /*!
     * Populate model with events matching the provided contact ID.
     *
     * \param contactId, identifies the contact whose events should be fetched.
     *
     * \return true if successful, otherwise false
     */
    bool getEvents(int contactId);

    /*!
     * Populate model with events matching the provided contact ID.
     *
     * \param contactId, identifies the contact whose events should be fetched.
     *
     * \return true if successful, otherwise false
     */
    bool getEvents(const QContactId &contactId);

    /*!
     * Populate model with events matching the provided recipient.
     *
     * \param recipient, identifies the contact whose events should be fetched.
     *
     * \return true if successful, otherwise false
     */
    bool getEvents(const Recipient &recipient);

private:
    Q_DECLARE_PRIVATE(SingleContactEventModel)
};

} // namespace CommHistory

#endif // COMMHISTORY_SINGLE_CONTACT_EVENT_MODEL_H

