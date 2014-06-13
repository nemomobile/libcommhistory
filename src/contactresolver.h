/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: John Brooks <john.brooks@jollamobile.com>
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

#ifndef COMMHISTORY_CONTACTRESOLVER_H
#define COMMHISTORY_CONTACTRESOLVER_H

#include <QObject>
#include "event.h"
#include "recipient.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class ContactResolverPrivate;

/* ContactResolver accepts a list of Recipient and asynchronously resolves
 * matching contacts.
 *
 * Recipient shares its data, including the resolved contact, with all instances
 * for the same address. ContactResolver will update that data as it resolves,
 * and emit a signal when all recipients in the list have been resolved.
 *
 * To ensure that all contacts are resolved for a list of Event, you can add
 * the recipients for each event to ContactResolver and wait for the finished
 * signal.
 */
class LIBCOMMHISTORY_EXPORT ContactResolver : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ContactResolver)

public:
    explicit ContactResolver(QObject *parent);

    /* Force resolving contacts even if already resolved */
    bool forceResolving() const;
    void setForceResolving(bool enabled);

    void add(const Recipient &recipient);
    void add(const RecipientList &recipients);
    template<typename T> void add(const T &value);
    template<typename T> void add(const QList<T> &value);

    bool isResolving() const;

signals:
    void finished();

private:
    ContactResolverPrivate *d_ptr;
};

template<typename T> void ContactResolver::add(const T &value)
{
    add(value.recipients());
}

template<typename T> void ContactResolver::add(const QList<T> &value)
{
    for (typename QList<T>::ConstIterator it = value.begin(); it != value.end(); it++) {
        add(it->recipients());
    }
}

}

#endif
