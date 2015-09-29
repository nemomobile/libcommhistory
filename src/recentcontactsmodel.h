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
#include <seasidecache.h>

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

    Q_PROPERTY(int requiredProperty READ requiredProperty WRITE setRequiredProperty)
    Q_PROPERTY(bool excludeFavorites READ excludeFavorites WRITE setExcludeFavorites)
    Q_PROPERTY(bool resolving READ resolving NOTIFY resolvingChanged)
    Q_ENUMS(RequiredPropertyType)

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

    enum RequiredPropertyType {
        NoPropertyRequired = 0,
        AccountUriRequired = SeasideCache::FetchAccountUri,
        PhoneNumberRequired = SeasideCache::FetchPhoneNumber,
        EmailAddressRequired = SeasideCache::FetchEmailAddress
    };

    /*!
     * Returns the property type(s) that contacts must possess to be included in the model.
     *
     * \return Property type mask
     */
    int requiredProperty() const;

    /*!
     * Set the property type(s) that contacts must possess to be included in the model.
     *
     * Valid values are a combination of: [
     *   CommRecentContactsModel.AccountUriRequired - contacts possessing an IM account are included,
     *   CommRecentContactsModel.PhoneNumberRequired - contacts possessing a phone number are included,
     *   CommRecentContactsModel.EmailAddressRequired - contacts possessing an email address account are included ]
     *
     * \param name Property type mask
     */
    void setRequiredProperty(int properties);

    /*!
     * Returns true if the model is set to exclude favorite contacts from results.
     */
    bool excludeFavorites() const;

    /*!
     * Set whether the model should exclude favorite contacts from results.
     */
    void setExcludeFavorites(bool exclude);

    /*!
     * Returns true if the model is engaged in resolving contacts, or false if all
     * relevant contacts have been resolved.
     */
    bool resolving() const;

Q_SIGNALS:
    void resolvingChanged();

private:
    Q_DECLARE_PRIVATE(RecentContactsModel)
};

} // namespace CommHistory

#endif // COMMHISTORY_RECENT_EVENTS_MODEL_H
