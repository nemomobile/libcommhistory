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

#include "contactlistener.h"
#include "contactlistener_p.h"

#include <QCoreApplication>
#include <QDebug>

#include <qtcontacts-extensions.h>

#include <QContactOnlineAccount>
#include <QContactPhoneNumber>
#include <QContactEmailAddress>
#include <QContactSyncTarget>

#include "commonutils.h"
#include "debug.h"

using namespace CommHistory;

Q_DECLARE_METATYPE(ContactListener::ContactAddress);
Q_DECLARE_METATYPE(QList<ContactListener::ContactAddress>);

Q_GLOBAL_STATIC(QWeakPointer<ContactListener>, contactListenerInstance);

typedef QPair<QString, QString> StringPair;

ContactListener::ContactListener(QObject *parent)
    : QObject(parent),
      d_ptr(new ContactListenerPrivate(this))
{
    qRegisterMetaType<QList<ContactListener::ContactAddress> >("QList<ContactAddress>");

    connect(d_ptr, SIGNAL(contactAlreadyInCache(quint32,QString,QList<ContactAddress>)),
            this, SIGNAL(contactUpdated(quint32,QString,QList<ContactAddress>)),
            Qt::QueuedConnection);
}

ContactListener::~ContactListener()
{
}

QSharedPointer<ContactListener> ContactListener::instance()
{
    QSharedPointer<ContactListener> result = contactListenerInstance->toStrongRef();
    if (!result) {
        result = QSharedPointer<ContactListener>(new ContactListener);
        *contactListenerInstance = result.toWeakRef();
    }

    return result;
}

ContactListenerPrivate::ContactListenerPrivate(ContactListener *q)
    : QObject(q), q_ptr(q)
{
    SeasideCache::registerChangeListener(this);
}

ContactListenerPrivate::~ContactListenerPrivate()
{
    SeasideCache::unregisterResolveListener(this);
    SeasideCache::unregisterChangeListener(this);
}

bool ContactListener::addressMatchesList(const QString &localUid,
                                         const QString &remoteUid,
                                         const QList< QPair<QString,QString> > &contactAddresses)
{
    QListIterator<StringPair> i(contactAddresses);
    while (i.hasNext()) {
        StringPair address = i.next();
        // Empty address.first is allowed to match phone number type localUids
        if ((address.first == localUid || (address.first.isEmpty() && localUidComparesPhoneNumbers(localUid)))
            && CommHistory::remoteAddressMatch(localUid, remoteUid, address.second, true)) {
            return true;
        }
    }

    return false;
}

bool ContactListener::addressMatchesList(const QString &localUid,
                                         const QString &remoteUid,
                                         const QList<ContactAddress> &contactAddresses)
{
    QListIterator<ContactAddress> i(contactAddresses);
    while (i.hasNext()) {
        ContactAddress address = i.next();
        if ((address.localUid == localUid || (address.localUid.isEmpty() && localUidComparesPhoneNumbers(localUid)))
            && CommHistory::remoteAddressMatch(localUid, remoteUid, address.remoteUid, true)) {
            return true;
        }
    }

    return false;
}

void ContactListener::resolveContact(const QString &localUid,
                                     const QString &remoteUid)
{
    Q_D(ContactListener);
    DEBUG() << Q_FUNC_INFO << localUid << remoteUid;

    const StringPair input(qMakePair(localUid, remoteUid));

    SeasideCache::CacheItem *item = 0;

    if (CommHistory::localUidComparesPhoneNumbers(localUid)) {
        d->m_pending.insert(qMakePair(QString(), remoteUid), input);
        item = SeasideCache::resolvePhoneNumber(d, remoteUid, true);
    } else {
        item = SeasideCache::resolveOnlineAccount(d, localUid, remoteUid, true);
    }

    if (item && (item->contactState == SeasideCache::ContactComplete)) {
        // This contact must be reported asynchronously
        emit d->contactAlreadyInCache(item->iid, d->contactName(item->contact), d->contactAddresses(item->contact));
    }
}

QString ContactListenerPrivate::contactName(const QContact &contact) const
{
    return SeasideCache::generateDisplayLabel(contact, SeasideCache::displayLabelOrder());
}

QList<ContactListener::ContactAddress> ContactListenerPrivate::contactAddresses(const QContact &contact) const
{
    QList<ContactAddress> addresses;

    foreach (const QContactOnlineAccount &account, contact.details<QContactOnlineAccount>()) {
        QString localUid = account.value<QString>(QContactOnlineAccount__FieldAccountPath);
        addresses += makeContactAddress(localUid, account.accountUri(), ContactListener::IMAccountType);
    }
    foreach (const QContactPhoneNumber &phoneNumber, contact.details<QContactPhoneNumber>()) {
        addresses += makeContactAddress(QString(), phoneNumber.number(), ContactListener::PhoneNumberType);
    }
    foreach (const QContactEmailAddress &emailAddress, contact.details<QContactEmailAddress>()) {
        addresses += makeContactAddress(QString::fromLatin1("email"), emailAddress.emailAddress(), ContactListener::EmailAddressType);
    }

    return addresses;
}

void ContactListenerPrivate::addressResolved(const QString &first, const QString &second,
                                             SeasideCache::CacheItem *item)
{
    Q_Q(ContactListener);

    if (item) {
        itemUpdated(item);
    } else {
        // This address could not be resolved
        StringPair address(qMakePair(first, second));

        QHash<StringPair, StringPair>::iterator it = m_pending.find(address);
        if (it != m_pending.end()) {
            // Report this address as unresolved
            address = *it;
            m_pending.erase(it);
        }

        emit q->contactUnknown(address);
    }
}

void ContactListenerPrivate::itemUpdated(SeasideCache::CacheItem *item)
{
    static const QString aggregateTarget(QString::fromLatin1("aggregate"));
    Q_Q(ContactListener);

    // Only aggregate contacts are relevant
    QContactSyncTarget syncTarget(item->contact.detail<QContactSyncTarget>());
    if (syncTarget.syncTarget() == aggregateTarget) {
        emit q->contactUpdated(item->iid, contactName(item->contact), contactAddresses(item->contact));
    }
}

void ContactListenerPrivate::itemAboutToBeRemoved(SeasideCache::CacheItem *item)
{
    Q_Q(ContactListener);
    emit q->contactRemoved(item->iid);
}

