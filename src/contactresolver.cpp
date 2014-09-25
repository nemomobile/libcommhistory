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

#include "contactresolver.h"

#include <QElapsedTimer>

#include "commonutils.h"
#include "debug.h"

using namespace CommHistory;

namespace CommHistory {

typedef QPair<QString, QString> UidPair;

class ContactResolverPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactResolver)

public:
    typedef ContactListener::ContactAddress ContactAddress;

    ContactResolver *q_ptr;
    QSharedPointer<ContactListener> listener;
    QList<Event> events; // events to be resolved

    // All uidpairs relevant to events
    QSet<UidPair> requestedAddresses;
    // Addresses with known contacts; in case of multiple contacts for
    // one address, the most recent contactUpdated one is kept.
    // Only addresses in requestedAddresses are tracked.
    QHash<UidPair, Event::Contact> resolvedAddresses;
    // Reverse mapping for resolvedAddresses
    QHash<quint32, QSet<UidPair> > resolvedIds;
    // Addresses still expecting a contactUpdated / contactUnknown
    QSet<UidPair> pendingAddresses;

    QElapsedTimer resolveTimer;

    explicit ContactResolverPrivate(ContactResolver *parent);

    void resolveEvent(const Event &event);
    void checkIfResolved();
    QList<Event> applyResolvedContacts() const;

    // Return a localUid/remoteUid pair in a form that can be
    // used for comparisons and set/hash lookups.
    static UidPair foldedAddress(const QString &localUid, const QString &remoteUid);
    static UidPair foldedAddress(const ContactAddress &address);
    static UidPair foldedEventAddress(const Event &event);
    static UidPair foldedEventAddress(const QString &localUid, const QString &remoteUid);

public slots:
    void contactUpdated(quint32 localId, const QString &name, const QList<ContactAddress> &addresses);
    void contactRemoved(quint32 localId);
    void contactUnknown(const QPair<QString, QString> &address);
};

} // namespace CommHistory

ContactResolver::ContactResolver(QObject *parent)
    : QObject(parent), d_ptr(new ContactResolverPrivate(this))
{
}

ContactResolverPrivate::ContactResolverPrivate(ContactResolver *parent)
    : QObject(parent), q_ptr(parent)
{
}

QList<Event> ContactResolver::events() const
{
    Q_D(const ContactResolver);
    return d->applyResolvedContacts();
}

bool ContactResolver::isResolving() const
{
    Q_D(const ContactResolver);
    return !d->events.isEmpty();
}

void ContactResolver::appendEvents(const QList<Event> &events)
{
    Q_D(ContactResolver);

    if (d->events.isEmpty())
        d->resolveTimer.start();

    foreach (const Event &event, events) {
        d->resolveEvent(event);
        d->events.append(event);
    }

    d->checkIfResolved();
}

void ContactResolver::prependEvents(const QList<Event> &events)
{
    Q_D(ContactResolver);

    if (d->events.isEmpty())
        d->resolveTimer.start();

    foreach (const Event &event, events) {
        d->resolveEvent(event);
        d->events.prepend(event);
    }

    d->checkIfResolved();
}

void ContactResolverPrivate::resolveEvent(const Event &event)
{
    if (event.localUid().isEmpty() || event.remoteUid().isEmpty())
        return;

    UidPair uidPair(foldedEventAddress(event));

    if (requestedAddresses.contains(uidPair))
        return;
    requestedAddresses.insert(uidPair);
    pendingAddresses.insert(uidPair);

    if (listener.isNull()) {
        listener = ContactListener::instance();
        connect(listener.data(), SIGNAL(contactUpdated(quint32,QString,QList<ContactAddress>)),
                SLOT(contactUpdated(quint32,QString,QList<ContactAddress>)));
        connect(listener.data(), SIGNAL(contactRemoved(quint32)), SLOT(contactRemoved(quint32)));
        connect(listener.data(), SIGNAL(contactUnknown(QPair<QString,QString>)),
                SLOT(contactUnknown(QPair<QString,QString>)));
    }
    listener->resolveContact(event.localUid(), event.remoteUid());
}

QList<Event> ContactResolverPrivate::applyResolvedContacts() const
{
    QList<Event> resolved = events;
    // Give each event the contact that was found for its address,
    // or an empty contact list if none was found.
    QList<Event>::iterator it = resolved.begin(), end = resolved.end();
    for ( ; it != end; ++it) {
        Event &event(*it);
        UidPair uidPair(foldedEventAddress(event));
        QHash<UidPair, Event::Contact>::const_iterator found = resolvedAddresses.find(uidPair);
        if (found != resolvedAddresses.end())
            event.setContacts(QList<Event::Contact>() << *found);
        else
            event.setContacts(QList<Event::Contact>());
    }
    return resolved;
}

UidPair ContactResolverPrivate::foldedAddress(const QString &localUid, const QString &remoteUid)
{
    if (localUid.isEmpty()) {
        QString remote = minimizePhoneNumber(remoteUid);
        if (remote.isEmpty())
            remote = remoteUid;
        return qMakePair(QString(), remote.toCaseFolded());
    }
    return qMakePair(localUid.toCaseFolded(),
                     remoteUid.toCaseFolded());
}

UidPair ContactResolverPrivate::foldedAddress(const ContactAddress &address)
{
    return foldedAddress(address.localUid, address.remoteUid);
}

UidPair ContactResolverPrivate::foldedEventAddress(const QString &localUid, const QString &remoteUid)
{
    if (localUidComparesPhoneNumbers(localUid))
        return foldedAddress(QString(), remoteUid);
    return qMakePair(localUid.toCaseFolded(),
                     remoteUid.toCaseFolded());
}

UidPair ContactResolverPrivate::foldedEventAddress(const Event &event)
{
    return foldedEventAddress(event.localUid(), event.remoteUid());
}

void ContactResolverPrivate::checkIfResolved()
{
    Q_Q(ContactResolver);

    if (events.isEmpty())
        return;

    if (!pendingAddresses.isEmpty())
        return;

    QList<Event> resolved = applyResolvedContacts();
    events.clear();

    qDebug() << "Resolved" << resolved.count() << "events in" << resolveTimer.elapsed() << "msec";

    emit q->eventsResolved(resolved);
    emit q->finished();
}

void ContactResolverPrivate::contactUpdated(quint32 localId, const QString &name, const QList<ContactAddress> &addresses)
{
    // normalized addresses for this contact that are relevant to
    // the events being resolved.
    QSet<UidPair> updated;

    foreach (const ContactAddress &address, addresses) {
        UidPair uidPair(foldedAddress(address));
        if (!requestedAddresses.contains(uidPair))
            continue;
        updated.insert(uidPair);

        if (resolvedAddresses.contains(uidPair)) {
            quint32 oldId = resolvedAddresses.value(uidPair).first;
            if (oldId != localId && resolvedIds.contains(oldId))
                resolvedIds[oldId].remove(uidPair);
        }

        Event::Contact contact(localId, name);
        resolvedAddresses.insert(uidPair, contact);
        pendingAddresses.remove(uidPair);
    }

    // Deal with addresses that no longer resolve to this contact
    foreach (const UidPair &uidPair, resolvedIds.value(localId)) {
        if (!updated.contains(uidPair))
            resolvedAddresses.remove(uidPair);
    }

    if (updated.isEmpty())
        resolvedIds.remove(localId);
    else
        resolvedIds.insert(localId, updated);

    checkIfResolved();
}
 
void ContactResolverPrivate::contactRemoved(quint32 localId)
{
    if (!resolvedIds.contains(localId))
        return;

    foreach (const UidPair &uidPair, resolvedIds.value(localId)) {
        resolvedAddresses.remove(uidPair);
    }

    resolvedIds.remove(localId);
}

void ContactResolverPrivate::contactUnknown(const QPair<QString, QString> &address)
{
    // ContactListener goes out of its way to give us an address in
    // raw event format instead of the kind given in ContactAddress,
    // so call foldedEventAddress rather than foldedAddress.
    UidPair uidPair = foldedEventAddress(address.first, address.second);
    if (pendingAddresses.remove(uidPair))
        checkIfResolved();
}

#include "contactresolver.moc"
