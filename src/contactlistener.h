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

#ifndef COMMHISTORY_CONTACTLISTENER_H
#define COMMHISTORY_CONTACTLISTENER_H

// QT includes
#include <QObject>
#include <QString>
#include <QList>
#include <QPair>
#include <QHash>

#include "libcommhistoryexport.h"

#include <seasidecache.h>

// contacts
#include <QContact>
#include <QContactId>

#ifdef USING_QTPIM
QTCONTACTS_USE_NAMESPACE
#else
QTM_USE_NAMESPACE
#endif

namespace CommHistory {

class LIBCOMMHISTORY_EXPORT ContactListener
    : public QObject,
      public SeasideCache::ResolveListener,
      public SeasideCache::ChangeListener
{
    Q_OBJECT

public:
    typedef QtContactsSqliteExtensions::ApiContactIdType ApiContactIdType;

    enum ContactAddressType {
        UnknownType = 0,
        IMAccountType,
        PhoneNumberType,
        EmailAddressType
    };

    struct ContactAddress {
        QString localUid;
        QString remoteUid;
        ContactAddressType type;

        QPair<QString, QString> uidPair() const { return qMakePair(localUid, remoteUid); }
    };

    template<typename T1, typename T2, typename T3>
    static ContactAddress makeContactAddress(T1 localUid, T2 remoteUid, T3 type) {
        ContactAddress addr;
        addr.localUid = localUid;
        addr.remoteUid = remoteUid;
        addr.type = type;
        return addr;
    }

    /*!
     *  \returns Contact listener
     */
    static QSharedPointer<ContactListener> instance();

    ~ContactListener();

    static bool addressMatchesList(const QString &localUid,
                                   const QString &remoteUid,
                                   const QList< QPair<QString,QString> > &contactAddresses);
    static bool addressMatchesList(const QString &localUid,
                                   const QString &remoteUid,
                                   const QList<ContactAddress> &contactAddresses);

    static int internalContactId(const ApiContactIdType &id);
    static int internalContactId(const QContact &contact);
    static ApiContactIdType apiContactId(int internalId);

    /**
     * Find a contact for (localUid, remoteUid), result provided via conactUpdate() signal.
     */
    void resolveContact(const QString &localUid,
                        const QString &remoteUid);

    /**
     * Get contact name from a QContact. Should have QContactName, QContactNickname,
     * and QContactPresence details. */
    QString contactName(const QContact &contact);

Q_SIGNALS:
    void contactUpdated(quint32 localId,
                        const QString &contactName,
                        const QList<ContactAddress> &contactAddresses);
    void contactRemoved(quint32 localId);
    void contactUnknown(const QPair<QString, QString> &address);

    // Private:
    void contactAlreadyInCache(quint32 localId,
                               const QString &contactName,
                               const QList<ContactAddress> &contactAddresses);

private:
    ContactListener(QObject *parent = 0);

    void init();

    QList<ContactAddress> contactAddresses(const QContact &contact) const;

    void addressResolved(const QString &first, const QString &second, SeasideCache::CacheItem *item);
    void itemUpdated(SeasideCache::CacheItem *item);
    void itemAboutToBeRemoved(SeasideCache::CacheItem *item);

private:
    static QWeakPointer<ContactListener> m_instance;
    bool m_initialized;
    QHash<QPair<QString, QString>, QPair<QString, QString> > m_pending;
};

}

#endif
