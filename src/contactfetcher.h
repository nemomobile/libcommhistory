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

#ifndef COMMHISTORY_CONTACTFETCHER_H
#define COMMHISTORY_CONTACTFETCHER_H

#include "recipient.h"
#include "libcommhistoryexport.h"

#include <QObject>

namespace CommHistory {

class ContactFetcherPrivate;

/* ContactFetcher accepts a list of contact IDs and fetches those contacts so that
 * their details are available from the contact cache.
 */
class LIBCOMMHISTORY_EXPORT ContactFetcher : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ContactFetcher)

public:
    explicit ContactFetcher(QObject *parent = 0);

    void add(int contactId);
    void add(const QContactId &contactId);
    void add(const Recipient &recipient);

    bool isFetching() const;

signals:
    void finished();

private:
    ContactFetcherPrivate *d_ptr;
};

}

#endif
