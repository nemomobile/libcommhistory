/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
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

#ifndef COMMHISTORY_CONTACTLISTENER_P_H
#define COMMHISTORY_CONTACTLISTENER_P_H

#include "contactlistener.h"
#include <QObject>
#include <seasidecache.h>

namespace CommHistory {

class ContactListenerPrivate
    : public QObject,
      public SeasideCache::ResolveListener,
      public SeasideCache::ChangeListener
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactListener)

public:
    typedef ContactListener::ContactAddress ContactAddress;

    template<typename T1, typename T2, typename T3> static
    ContactAddress makeContactAddress(T1 localUid, T2 remoteUid, T3 type) {
        ContactAddress addr;
        addr.localUid = localUid;
        addr.remoteUid = remoteUid;
        addr.type = type;
        return addr;
    }

    ContactListenerPrivate(ContactListener *q);
    virtual ~ContactListenerPrivate();

    QHash<QPair<QString, QString>, QPair<QString, QString> > m_pending;

    QList<ContactAddress> contactAddresses(const QContact &contact) const;
    QString contactName(const QContact &contact) const;

Q_SIGNALS:
    // Private:
    void contactAlreadyInCache(quint32 localId,
                               const QString &contactName,
                               const QList<ContactAddress> &contactAddresses);
    void contactAlreadyUnknown(const QPair<QString,QString> &address);

protected:
    void addressResolved(const QString &first, const QString &second, SeasideCache::CacheItem *item);
    void itemUpdated(SeasideCache::CacheItem *item);
    void itemAboutToBeRemoved(SeasideCache::CacheItem *item);

private:
    ContactListener *q_ptr;
};

}

#endif
