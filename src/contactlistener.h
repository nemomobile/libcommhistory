/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef COMMHISTORY_CONTACTLISTENER_H
#define COMMHISTORY_CONTACTLISTENER_H

// QT includes
#include <QObject>
#include <QString>
#include <QList>
#include <QPair>

#include "libcommhistoryexport.h"

namespace CommHistory {

class ContactListenerPrivate;

class LIBCOMMHISTORY_EXPORT ContactListener : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ContactListener)

public:
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

    /*!
     *  \returns Contact listener
     */
    static QSharedPointer<ContactListener> instance();
    virtual ~ContactListener();

    static bool addressMatchesList(const QString &localUid,
                                   const QString &remoteUid,
                                   const QList< QPair<QString,QString> > &contactAddresses);
    static bool addressMatchesList(const QString &localUid,
                                   const QString &remoteUid,
                                   const QList<ContactAddress> &contactAddresses);

    /**
     * Find a contact for (localUid, remoteUid), result provided via contactUpdate() signal.
     */
    void resolveContact(const QString &localUid,
                        const QString &remoteUid);

Q_SIGNALS:
    void contactUpdated(quint32 localId,
                        const QString &contactName,
                        const QList<ContactAddress> &contactAddresses);
    void contactRemoved(quint32 localId);
    void contactUnknown(const QPair<QString, QString> &address);

private:
    ContactListenerPrivate *d_ptr;

    ContactListener(QObject *parent = 0);
};

}

#endif
