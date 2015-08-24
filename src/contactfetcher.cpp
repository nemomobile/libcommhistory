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

#include "contactfetcher.h"
#include "contactresolver.h"

#include "commonutils.h"
#include "debug.h"

using namespace CommHistory;

namespace CommHistory {

class ContactFetcherPrivate
    : public QObject,
      public SeasideCache::ChangeListener
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactFetcher)

public:
    ContactFetcher *q_ptr;
    ContactResolver *m_resolver;
    QSet<int> m_waiting;
    QSet<Recipient> m_resolving;
    bool m_fetching;

    explicit ContactFetcherPrivate(ContactFetcher *q);
    ~ContactFetcherPrivate();

    void fetch(int contactId);
    void fetch(const QContactId &contactId);
    void fetch(const Recipient &recipient);

    void checkIfFinishedAsynchronously();

    virtual void itemUpdated(SeasideCache::CacheItem *item);
    virtual void itemAboutToBeRemoved(SeasideCache::CacheItem *) {}

public slots:
    void resolverFinished();
    bool checkIfFinished();
};

ContactFetcherPrivate::ContactFetcherPrivate(ContactFetcher *parent)
    : QObject(parent)
    , q_ptr(parent)
    , m_resolver(0)
    , m_fetching(false)
{
    SeasideCache::registerChangeListener(this);
}

ContactFetcherPrivate::~ContactFetcherPrivate()
{
    SeasideCache::unregisterChangeListener(this);
}

void ContactFetcherPrivate::fetch(int contactId)
{
    if (contactId > 0) {
        SeasideCache::CacheItem *item = SeasideCache::itemById(contactId, true);
        if (!item || item->contactState != SeasideCache::ContactComplete) {
            m_waiting.insert(contactId);
        }
    }
}

void ContactFetcherPrivate::fetch(const QContactId &contactId)
{
    fetch(SeasideCache::internalId(contactId));
}

void ContactFetcherPrivate::fetch(const Recipient &recipient)
{
    if (!recipient.isContactResolved()) {
        if (!m_resolver) {
            m_resolver = new ContactResolver(this);
            connect(m_resolver, SIGNAL(finished()), SLOT(resolverFinished()));
        }

        m_resolver->add(recipient);
        m_resolving.insert(recipient);
    } else {
        fetch(recipient.contactId());
    }
}

void ContactFetcherPrivate::checkIfFinishedAsynchronously()
{
    if (!m_fetching) {
        m_fetching = true;

        if (m_waiting.isEmpty() && m_resolving.isEmpty()) {
            bool ok = metaObject()->invokeMethod(this, "checkIfFinished", Qt::QueuedConnection);
            Q_UNUSED(ok);
            Q_ASSERT(ok);
        }
    }
}

void ContactFetcherPrivate::itemUpdated(SeasideCache::CacheItem *item)
{
    if (item->contactState == SeasideCache::ContactComplete && m_waiting.contains(item->iid)) {
        // This item has been fetched into the cache
        m_waiting.remove(item->iid);
        checkIfFinished();
    }
}

void ContactFetcherPrivate::resolverFinished()
{
    // Fetch the contacts that we resolved the addresses to
    foreach (const Recipient &recipient, m_resolving)
        fetch(recipient.contactId());

    m_resolving.clear();
    checkIfFinished();
}

bool ContactFetcherPrivate::checkIfFinished()
{
    Q_Q(ContactFetcher);
    if (m_fetching && m_waiting.isEmpty() && m_resolving.isEmpty()) {
        m_fetching = false;
        emit q->finished();
        return true;
    }
    return false;
}

ContactFetcher::ContactFetcher(QObject *parent)
    : QObject(parent), d_ptr(new ContactFetcherPrivate(this))
{
}

void ContactFetcher::add(int contactId)
{
    Q_D(ContactFetcher);
    d->fetch(contactId);
    d->checkIfFinishedAsynchronously();
}

void ContactFetcher::add(const QContactId &contactId)
{
    Q_D(ContactFetcher);
    d->fetch(contactId);
    d->checkIfFinishedAsynchronously();
}

void ContactFetcher::add(const Recipient &recipient)
{
    Q_D(ContactFetcher);
    d->fetch(recipient);
    d->checkIfFinishedAsynchronously();
}

bool ContactFetcher::isFetching() const
{
    Q_D(const ContactFetcher);
    return d->m_fetching;
}

} // namespace CommHistory

#include "contactfetcher.moc"
