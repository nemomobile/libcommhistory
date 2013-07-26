/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd. <matthew.vogt@jollamobile.com>
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

#ifndef COMMHISTORY_RECENT_EVENTS_MODEL_H
#define COMMHISTORY_RECENT_EVENTS_MODEL_H

#include "eventmodel.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class RecentContactsModelPrivate;

/*!
 * \class RecentContactsModel
 * \brief Model containing the most recent event for each of the contacts
 *        most recently communicated with.
 * e.g. phone number or IM user id
 */
class LIBCOMMHISTORY_EXPORT RecentContactsModel : public EventModel
{
    Q_OBJECT

    Q_PROPERTY(QString selectionProperty READ selectionProperty WRITE setSelectionProperty)

public:
    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    explicit RecentContactsModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~RecentContactsModel();

    /*!
     * Populate model with existing events.
     *
     * \return true if successful, otherwise false
     */
    Q_INVOKABLE bool getEvents();

    /*!
     * Return the name of the property type that contacts must possess to be included in the model.
     *
     * \return Property type name
     */
    QString selectionProperty() const;

    /*!
     * Set the property type that contacts must possess to be included in the model.
     *
     * Valid values are: [ 'accountUri' - contacts must possess an IM account,
     *                     'phoneNumber' - contacts must possess a phone number,
     *                     'emailAddress' - contacts must possess an email address ]
     *
     * The property names correspond to the property names by which these attributes
     * are exposed in 'org.nemomobile.contacts.Person'.
     *
     * \param name Property type name
     */
    void setSelectionProperty(const QString &name);

private:
    Q_DECLARE_PRIVATE(RecentContactsModel);
};

} // namespace CommHistory

#endif // COMMHISTORY_RECENT_EVENTS_MODEL_H
