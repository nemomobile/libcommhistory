/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jolla.com>
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

#ifndef COMMHISTORY_CONTACTLISTENER_H
#define COMMHISTORY_CONTACTLISTENER_H

#include <QObject>
#include <QSharedPointer>
#include "libcommhistoryexport.h"
#include "recipient.h"

namespace CommHistory {

class ContactListenerPrivate;

class LIBCOMMHISTORY_EXPORT ContactListener : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ContactListener)

public:
    static QSharedPointer<ContactListener> instance();
    virtual ~ContactListener();

signals:
    /* Emitted after the contact ID of a resolved Recipient is changed
     *
     * This signal is emitted when the contact ID of a Recipient which was
     * previously resolved (i.e. passed through ContactResolver) is changed.
     * This includes recipients that didn't resolve to any contact, but does
     * _not_ include recipients that have never tried to resolve.
     *
     * Changes to multiple recipients may be combined into a single signal
     * for efficiency.
     *
     * This signal is not emitted for changes to the contacts' details, unless
     * those changes resulted in a change of the matching contact ID.
     */
    void contactChanged(const RecipientList &recipients);

    /* Emitted after the visible information about a resolved contact is changed
     *
     * This signal is emitted when the exported contact details (currently
     * only contactName) are changed for a Recipient. Changes to contact
     * details that are not exported by Recipient are not reported.
     *
     * Because changing contacts implies a change of contact details, this
     * signal is also emitted in all cases where contactChanged is emitted.
     * It is safe to use this signal instead of contactChanged to cover both
     * cases efficiently.
     */
    void contactInfoChanged(const RecipientList &recipients);

    /* Emitted any time a contact is modified
     *
     * This signal is emitted after any modification to a contact, including
     * modifications that don't change any properties commhistory exports.
     *
     * This can be useful for monitoring other changes on the contacts resolved
     * from recipients.
     */
    void contactDetailsChanged(const RecipientList &recipients);

private:
    ContactListenerPrivate *d_ptr;

    ContactListener(QObject *parent = 0);
};

}

#endif
