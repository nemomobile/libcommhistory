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

#include "debug.h"

using namespace CommHistory;

namespace CommHistory {

class ContactResolverPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactResolver)

public:
    typedef ContactListener::ContactAddress ContactAddress;

    struct PendingEvent {
        Event event;
        bool resolved;

        PendingEvent(const Event &e)
            : event(e), resolved(false)
        { }
    };

    ContactResolver *q_ptr;
    QSharedPointer<ContactListener> listener;
    QList<PendingEvent> events;
    // Cache of localUid+remoteUid from events and the resulting contact, kept up to date with changes.
    QHash<QPair<QString,QString>, Event::Contact> contactCache;
    QSet<QPair<QString,QString> > pendingAddresses;

    QElapsedTimer resolveTimer;

    explicit ContactResolverPrivate(ContactResolver *parent);

    bool resolveEvent(PendingEvent &event);
    void checkIfResolved();

public slots:
    void contactUpdated(quint32 localId, const QString &name, const QList<ContactAddress> &addresses);
    void contactRemoved(quint32 localId);
    void contactUnknown(const QPair<QString,QString> &address);
};

}

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
    QList<Event> re;
    re.reserve(d->events.size());
    foreach (const ContactResolverPrivate::PendingEvent &pe, d->events)
        re.append(pe.event);
    return re;
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

    bool resolved = true;
    foreach (const Event &event, events) {
        ContactResolverPrivate::PendingEvent e(event);
        resolved &= d->resolveEvent(e);
        d->events.append(e);
    }

    if (resolved)
        d->checkIfResolved();
}

void ContactResolver::prependEvents(const QList<Event> &events)
{
    Q_D(ContactResolver);

    if (d->events.isEmpty())
        d->resolveTimer.start();

    bool resolved = true;
    foreach (const Event &event, events) {
        ContactResolverPrivate::PendingEvent e(event);
        resolved &= d->resolveEvent(e);
        d->events.prepend(e);
    }

    if (resolved)
        d->checkIfResolved();
}

// Returns true if resolved immediately
bool ContactResolverPrivate::resolveEvent(PendingEvent &e)
{
    if (e.event.localUid().isEmpty() || e.event.remoteUid().isEmpty()) {
        e.event.setContacts(QList<Event::Contact>());
        e.resolved = true;
        return true;
    }

    QPair<QString,QString> uidPair(e.event.localUid(), e.event.remoteUid());
    QHash<QPair<QString,QString>, Event::Contact>::iterator it = contactCache.find(uidPair);
    if (it != contactCache.end()) {
        e.event.setContacts(QList<Event::Contact>() << *it);
        e.resolved = true;
        return true;
    }

    if (pendingAddresses.contains(uidPair))
        return false;
    pendingAddresses.insert(uidPair);

    if (listener.isNull()) {
        listener = ContactListener::instance();
        connect(listener.data(), SIGNAL(contactUpdated(quint32,QString,QList<ContactAddress>)),
                SLOT(contactUpdated(quint32,QString,QList<ContactAddress>)));
        connect(listener.data(), SIGNAL(contactRemoved(quint32)), SLOT(contactRemoved(quint32)));
        connect(listener.data(), SIGNAL(contactUnknown(QPair<QString,QString>)),
                SLOT(contactUnknown(QPair<QString,QString>)));
    }

    listener->resolveContact(e.event.localUid(), e.event.remoteUid());
    return false;
}

void ContactResolverPrivate::checkIfResolved()
{
    Q_Q(ContactResolver);

    if (events.isEmpty())
        return;

    foreach (const PendingEvent &e, events) {
        if (!e.resolved)
            return;
    }

    QList<Event> resolved;
    resolved.reserve(events.size());
    foreach (const PendingEvent &e, events)
        resolved.append(e.event);
    events.clear();

    qDebug() << "Resolved" << resolved.count() << "events in" << resolveTimer.elapsed() << "msec";

    emit q->eventsResolved(resolved);
    emit q->finished();
}

void ContactResolverPrivate::contactUpdated(quint32 localId, const QString &name, const QList<ContactAddress> &addresses)
{
    QHash<QPair<QString,QString>, Event::Contact>::iterator cacheIt = contactCache.begin();
    while (cacheIt != contactCache.end()) {
        const QPair<QString, QString> &uidPair(cacheIt.key());
        Event::Contact &contact(cacheIt.value());

        if (ContactListener::addressMatchesList(uidPair.first,  // local id
                                                uidPair.second, // remote id
                                                addresses)) {
            if (quint32(contact.first) == localId) {
                // change name for existing contact
                contact.second = name;
            } else {
                qWarning() << "Duplicate contact match for address" << uidPair.first << uidPair.second << "with contacts" << contact.first << localId;
            }
        } else if (quint32(contact.first) == localId) {
            // delete our record since the address doesn't match anymore
            cacheIt = contactCache.erase(cacheIt);
            continue;
        }

        cacheIt++;
    }

    bool updated = false;
    for (QList<PendingEvent>::iterator it = events.begin(), end = events.end(); it != end; it++) {
        PendingEvent &e = *it;
        QPair<QString,QString> uidPair(e.event.localUid(), e.event.remoteUid());

        if (ContactListener::addressMatchesList(uidPair.first, uidPair.second, addresses)) {
            Event::Contact contact(localId, name);
            contactCache.insert(uidPair, contact);
            e.event.setContacts(QList<Event::Contact>() << contact);
            if (!e.resolved) {
                pendingAddresses.remove(uidPair);
                e.resolved = true;
                updated = true;
            }
        }
    }

    if (updated)
        checkIfResolved();
}

void ContactResolverPrivate::contactRemoved(quint32 localId)
{
    QHash<QPair<QString,QString>, Event::Contact>::iterator cacheIt = contactCache.begin();
    while (cacheIt != contactCache.end()) {
        Event::Contact &contact(cacheIt.value());
        if (quint32(contact.first) == localId) {
            cacheIt = contactCache.erase(cacheIt);
        } else {
            cacheIt++;
        }
    }

    for (QList<PendingEvent>::iterator it = events.begin(), end = events.end(); it != end; it++) {
        PendingEvent &e = *it;
        if (quint32(e.event.contactId()) == localId) {
            e.event.setContacts(QList<Event::Contact>());
        }
    }
}

void ContactResolverPrivate::contactUnknown(const QPair<QString,QString> &address)
{
    QList<QPair<QString,QString> > addresses;
    addresses << address;

    bool updated = false;
    for (QList<PendingEvent>::iterator it = events.begin(), end = events.end(); it != end; it++) {
        PendingEvent &e = *it;
        QPair<QString,QString> uidPair(e.event.localUid(), e.event.remoteUid());

        if (ContactListener::addressMatchesList(uidPair.first, uidPair.second, addresses)) {
            contactCache.insert(uidPair, Event::Contact(0, QString()));
            e.event.setContacts(QList<Event::Contact>());
            if (!e.resolved) {
                pendingAddresses.remove(uidPair);
                e.resolved = true;
                updated = true;
            }
        }
    }

    if (updated)
        checkIfResolved();
}

#include "contactresolver.moc"

