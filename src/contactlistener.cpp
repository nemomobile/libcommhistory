/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
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

#include "contactlistener.h"

#include <QCoreApplication>
#include <QDebug>

#include <qtcontacts-extensions.h>
#include <seasidecache.h>

#include <QContactOnlineAccount>
#include <QContactPhoneNumber>
#include <QContactEmailAddress>
#include <QContactSyncTarget>

#include "commonutils.h"
#include "contactresolver.h"
#include "debug.h"

namespace CommHistory {

class ContactListenerPrivate
    : public QObject,
      public SeasideCache::ChangeListener
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactListener)

public:
    ContactListenerPrivate(ContactListener *q);
    virtual ~ContactListenerPrivate();

    ContactResolver *retryResolver;
    QList<Recipient> retryRecipients;

private slots:
    void retryFinished();
    void resolveAgain(const CommHistory::Recipient &recipient);

protected:
    void itemUpdated(SeasideCache::CacheItem *item);
    void itemAboutToBeRemoved(SeasideCache::CacheItem *item);

private:
    ContactListener *q_ptr;
};

}

using namespace CommHistory;

Q_GLOBAL_STATIC(QWeakPointer<ContactListener>, contactListenerInstance);

ContactListener::ContactListener(QObject *parent)
    : QObject(parent),
      d_ptr(new ContactListenerPrivate(this))
{
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
    : QObject(q)
    , retryResolver(0)
    , q_ptr(q)
{
    SeasideCache::registerChangeListener(this);
}

ContactListenerPrivate::~ContactListenerPrivate()
{
    SeasideCache::unregisterChangeListener(this);
}

void ContactListenerPrivate::resolveAgain(const Recipient &recipient)
{
    if (!retryResolver) {
        retryResolver = new ContactResolver(this);
        retryResolver->setForceResolving(true);
        connect(retryResolver, SIGNAL(finished()), SLOT(retryFinished()));
    }
    retryRecipients.append(recipient);
    retryResolver->add(recipient);
}

void ContactListenerPrivate::retryFinished()
{
    Q_Q(ContactListener);
    emit q->contactChanged(retryRecipients);
    emit q->contactInfoChanged(retryRecipients);
    emit q->contactDetailsChanged(retryRecipients);
    retryResolver->deleteLater();
    retryResolver = 0;
    retryRecipients.clear();
}

static QString contactName(const QContact &contact)
{
    return SeasideCache::generateDisplayLabel(contact, SeasideCache::displayLabelOrder());
}

static bool recipientMatchesDetails(const Recipient &recipient, const QList<Recipient> &addresses, const QList<QPair<QString, quint32> > &phoneNumbers)
{
    if (recipient.isPhoneNumber()) {
        for (QList<QPair<QString, quint32> >::const_iterator it = phoneNumbers.begin(), end = phoneNumbers.end(); it != end; ++it) {
            if (recipient.matchesPhoneNumber(*it))
                return true;
        }
    } else {
        foreach (const Recipient &address, addresses) {
            if (recipient.matches(address))
                return true;
        }
    }

    return false;
}

void ContactListenerPrivate::itemUpdated(SeasideCache::CacheItem *item)
{
    Q_Q(ContactListener);

    // Only aggregate contacts are relevant
    static const QString aggregateTarget(QString::fromLatin1("aggregate"));
    QContactSyncTarget syncTarget(item->contact.detail<QContactSyncTarget>());
    if (syncTarget.syncTarget() != aggregateTarget)
        return;

    // Make a list of Recipient from the contacts addresses to compare against
    QList<Recipient> addresses;
    foreach (const QContactOnlineAccount &account, item->contact.details<QContactOnlineAccount>()) {
        addresses.append(Recipient(account.value<QString>(QContactOnlineAccount__FieldAccountPath), account.accountUri()));
    }

    QList<QPair<QString, quint32> > phoneNumbers;
    foreach (const QContactPhoneNumber &phoneNumber, item->contact.details<QContactPhoneNumber>()) {
        phoneNumbers.append(Recipient::phoneNumberMatchDetails(phoneNumber.number()));
    }

    QString displayName = contactName(item->contact);

    // Check that all recipients resolved to this contact still match
    QList<Recipient> recipients = Recipient::recipientsForContact(item->iid);
    foreach (const Recipient &recipient, recipients) {
        if (!recipientMatchesDetails(recipient, addresses, phoneNumbers)) {
            DEBUG() << "Recipient" << recipient.remoteUid() << "no longer matches contact" << item->iid;
            // Try to resolve again to find a new match before emitting signals
            resolveAgain(recipient);
        } else {
            if (recipient.contactName() != displayName) {
                recipient.setResolvedContact(item->iid, displayName);
                // XXX combine signal
                emit q->contactInfoChanged(recipient);
            }

            emit q->contactDetailsChanged(recipient);
        }
    }

    // Check all recipients that resolved to no match against these addresses
    recipients = Recipient::recipientsForContact(0);
    foreach (const Recipient &recipient, recipients) {
        if (recipientMatchesDetails(recipient, addresses, phoneNumbers)) {
            DEBUG() << "Recipient" << recipient << "now resolves to updated contact" << item->iid;
            recipient.setResolvedContact(item->iid, displayName);
            // XXX combine signal
            // XXX must emit details signal too
            emit q->contactChanged(recipient);
            emit q->contactInfoChanged(recipient);
            emit q->contactDetailsChanged(recipient);
        }
    }
}

void ContactListenerPrivate::itemAboutToBeRemoved(SeasideCache::CacheItem *item)
{
    QList<Recipient> recipients = Recipient::recipientsForContact(item->iid);
    foreach (const Recipient &recipient, recipients) {
        DEBUG() << "Recipient" << recipient << "matched removed contact" << item->iid;
        // Delay retrying to be sure the contact is removed from cache
        metaObject()->invokeMethod(this, "resolveAgain", Qt::QueuedConnection, Q_ARG(CommHistory::Recipient, recipient));
    }
}

#include "contactlistener.moc"

