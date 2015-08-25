/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
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

#include "recipient.h"
#include "commonutils.h"
#include <QSet>
#include <QHash>
#include <QDebug>

namespace {

bool initializeTypes()
{
    qRegisterMetaType<CommHistory::Recipient>();
    qRegisterMetaType<CommHistory::RecipientList>();
    return true;
}

QString minimizeRemoteUid(const QString &remoteUid, bool isPhoneNumber)
{
    // For non-standard localUid values, we still need a comparable value
    return isPhoneNumber ? CommHistory::minimizePhoneNumber(remoteUid).toLower() : remoteUid.toLower();
}

quint32 addressFlagValues(quint64 statusFlags)
{
    return statusFlags & (QContactStatusFlags::HasPhoneNumber | QContactStatusFlags::HasEmailAddress | QContactStatusFlags::HasOnlineAccount);
}

}

bool recipient_initialized = initializeTypes();

namespace CommHistory {

class RecipientPrivate
{
public:
    QString localUid;
    QString remoteUid;
    SeasideCache::CacheItem* item;
    bool isResolved;
    bool isPhoneNumber;
    QString minimizedRemoteUid;
    quint32 localUidHash;
    quint32 remoteUidHash;
    quint32 contactNameHash;
    quint32 addressFlags;

    RecipientPrivate(const QString &localUid, const QString &remoteUid);
    ~RecipientPrivate();

    static QSharedPointer<RecipientPrivate> get(const QString &localUid, const QString &remoteUid);
};

}

using namespace CommHistory;

typedef QHash<QPair<QString,QString>,WeakRecipient> RecipientUidMap;
typedef QMultiHash<int,WeakRecipient> RecipientContactMap;

Q_GLOBAL_STATIC(RecipientUidMap, recipientInstances);
Q_GLOBAL_STATIC(RecipientContactMap, recipientContactMap);
Q_GLOBAL_STATIC_WITH_ARGS(QSharedPointer<RecipientPrivate>, sharedNullRecipient, (new RecipientPrivate(QString(), QString())));

Recipient::Recipient()
{
    d = *sharedNullRecipient;
}

Recipient::Recipient(const QString &localUid, const QString &remoteUid)
    : d(RecipientPrivate::get(localUid, remoteUid))
{
}

Recipient::Recipient(const Recipient &o)
    : d(o.d)
{
}

Recipient::Recipient(const WeakRecipient &weak)
    : d(weak)
{
    if (!d)
        d = *sharedNullRecipient;
}

Recipient &Recipient::operator=(const Recipient &o)
{
    d = o.d;
    return *this;
}

Recipient::~Recipient()
{
}

RecipientPrivate::RecipientPrivate(const QString &local, const QString &remote)
    : localUid(local)
    , remoteUid(remote)
    , item(0)
    , isResolved(false)
    , isPhoneNumber(localUidComparesPhoneNumbers(localUid))
    // The following members could be initialized on-demand, but that appears to be slower overall
    , minimizedRemoteUid(::minimizeRemoteUid(remoteUid, isPhoneNumber))
    , localUidHash(qHash(localUid))
    , remoteUidHash(qHash(minimizedRemoteUid))
    , contactNameHash(0)
    , addressFlags(0)
{
}

RecipientPrivate::~RecipientPrivate()
{
    if (!recipientInstances.isDestroyed()) {
        recipientInstances->remove(qMakePair(localUid, remoteUid.toLower()));
    }
}

QSharedPointer<RecipientPrivate> RecipientPrivate::get(const QString &localUid, const QString &remoteUid)
{
    if (localUid.isEmpty() && remoteUid.isEmpty()) {
        return *sharedNullRecipient;
    }

    const QPair<QString,QString> uids = qMakePair(localUid, remoteUid.toLower());
    QSharedPointer<RecipientPrivate> instance = recipientInstances->value(uids);
    if (!instance) {
        instance = QSharedPointer<RecipientPrivate>(new RecipientPrivate(localUid, remoteUid));
        recipientInstances->insert(uids, instance);
    }
    return instance;
}

bool Recipient::isNull() const
{
    return d->localUid.isEmpty() && d->remoteUid.isEmpty();
}

QString Recipient::displayName() const
{
    return d->item ? d->item->displayLabel : d->remoteUid;
}

QString Recipient::localUid() const
{
    return d->localUid;
}

QString Recipient::remoteUid() const
{
    return d->remoteUid;
}

bool Recipient::isPhoneNumber() const
{
    return d->isPhoneNumber;
}

QString Recipient::minimizedPhoneNumber() const
{
    return d->isPhoneNumber ? CommHistory::minimizePhoneNumber(d->remoteUid) : QString();
}

QString Recipient::minimizedRemoteUid() const
{
    return d->minimizedRemoteUid;
}

// Because all equal instances share a dptr, this is a very fast comparison
bool Recipient::operator==(const Recipient &o) const
{
    return d == o.d;
}

bool Recipient::matches(const Recipient &o) const
{
    if (d == o.d)
        return true;
    if (d->isPhoneNumber != o.d->isPhoneNumber)
        return false;
    // For phone numbers, we ignore localUid comparison - may have to change in future?
    if (!d->isPhoneNumber && d->localUidHash != o.d->localUidHash)
        return false;
    if (d->remoteUidHash != o.d->remoteUidHash)
        return false;
    if (!d->isPhoneNumber && d->localUid != o.d->localUid)
        return false;
    return d->minimizedRemoteUid == o.d->minimizedRemoteUid;
}

bool Recipient::isSameContact(const Recipient &o) const
{
    if (d == o.d)
        return true;
    if (d->isResolved && o.d->isResolved && (d->item || o.d->item))
        return d->item == o.d->item;
    return matches(o);
}

bool Recipient::matchesRemoteUid(const QString &o) const
{
    const QString minimizedMatch(::minimizeRemoteUid(o, d->isPhoneNumber));
    return d->minimizedRemoteUid == minimizedMatch;
}

bool Recipient::matchesPhoneNumber(const QPair<QString, quint32> &phoneNumber) const
{
    if (!d->isPhoneNumber)
        return false;
    if (d->remoteUidHash != phoneNumber.second)
        return false;
    return d->minimizedRemoteUid == phoneNumber.first;
}

bool Recipient::matchesAddressFlags(quint64 flags) const
{
    if (!d->item)
        return false;
    if (addressFlagValues(flags) == 0)
        return true;
    return d->item->statusFlags & flags;
}

int Recipient::contactId() const
{
    return d->item ? d->item->iid : 0;
}

QString Recipient::contactName() const
{
    return d->item ? d->item->displayLabel : QString();
}

bool Recipient::isContactResolved() const
{
    return d->isResolved;
}

bool Recipient::setResolved(SeasideCache::CacheItem *item) const
{
    if (d->isResolved && item == d->item)
        return false;

    if (d->isResolved && d->item)
        recipientContactMap->remove(d->item->iid, d);

    recipientContactMap->insert(item ? item->iid : 0, d.toWeakRef());

    d->isResolved = true;
    d->item = item;
    d->contactNameHash = item ? qHash(item->displayLabel) : 0;
    d->addressFlags = item ? addressFlagValues(item->statusFlags) : 0;
    return true;
}

void Recipient::setUnresolved() const
{
    if (!d->isResolved)
        return;

    if (d->item)
        recipientContactMap->remove(d->item->iid, d);

    d->isResolved = false;
    d->item = 0;
    d->contactNameHash = 0;
    d->addressFlags = 0;
}

bool Recipient::contactUpdateIsSignificant() const
{
    if (d->item) {
        // The contact display label may have been updated
        const quint32 hash(qHash(d->item->displayLabel));
        const quint32 addressFlags(addressFlagValues(d->item->statusFlags));
        if (hash != d->contactNameHash || addressFlags != d->addressFlags) {
            d->contactNameHash = hash;
            d->addressFlags = addressFlags;
            return true;
        }
    }
    return false;
}

QList<Recipient> Recipient::recipientsForContact(int contactId)
{
    QList<Recipient> re;
    RecipientContactMap::iterator it = recipientContactMap->find(contactId);
    for (; it != recipientContactMap->end() && it.key() == contactId; ) {
        if (!*it) {
            it = recipientContactMap->erase(it);
            continue;
        }

        re.append(Recipient(*it));
        it++;
    }
    return re;
}

QPair<QString, quint32> Recipient::phoneNumberMatchDetails(const QString &s)
{
    const QString minimized(minimizeRemoteUid(s, true));
    return qMakePair(minimized, qHash(minimized));
}

RecipientList::RecipientList()
{
}

RecipientList::RecipientList(const Recipient &recipient)
{
    m_recipients << recipient;
}

RecipientList::RecipientList(const QList<Recipient> &r)
    : m_recipients(r)
{
}

RecipientList RecipientList::fromUids(const QString &localUid, const QStringList &remoteUids)
{
    RecipientList re;
    foreach (const QString &remoteUid, remoteUids)
        re << Recipient(localUid, remoteUid);
    return re;
}

RecipientList RecipientList::fromContact(int contactId)
{
    return fromCacheItem(SeasideCache::itemById(contactId, false));
}

RecipientList RecipientList::fromContact(const QContactId &contactId)
{
    return fromCacheItem(SeasideCache::itemById(contactId, false));
}

RecipientList RecipientList::fromCacheItem(const SeasideCache::CacheItem *item)
{
    RecipientList re;
    if (item && item->contactState == SeasideCache::ContactComplete) {
        foreach (const QContactOnlineAccount &account, item->contact.details<QContactOnlineAccount>()) {
            re << Recipient(account.value<QString>(QContactOnlineAccount__FieldAccountPath), account.accountUri());
        }
        foreach (const QContactPhoneNumber &phoneNumber, item->contact.details<QContactPhoneNumber>()) {
            re << Recipient(QString(), phoneNumber.number());
        }
    }
    return re;
}

bool RecipientList::isEmpty() const
{
    return m_recipients.isEmpty();
}

int RecipientList::size() const
{
    return m_recipients.size();
}

QList<Recipient> RecipientList::recipients() const
{
    return m_recipients;
}

QStringList RecipientList::displayNames() const
{
    QStringList re;
    re.reserve(m_recipients.size());
    foreach (const Recipient &r, m_recipients)
        re.append(r.displayName());
    return re;
}

// XXX flawed, shouldn't be around
QList<int> RecipientList::contactIds() const
{
    QSet<int> re;
    re.reserve(m_recipients.size());
    foreach (const Recipient &r, m_recipients) {
        if (r.contactId() > 0)
            re.insert(r.contactId());
    }
    return re.toList();
}

QStringList RecipientList::remoteUids() const
{
    QStringList re;
    re.reserve(m_recipients.size());
    foreach (const Recipient &r, m_recipients)
        re.append(r.remoteUid());
    return re;
}

bool RecipientList::operator==(const RecipientList &o) const
{
    if (o.m_recipients.size() != m_recipients.size())
        return false;

    QList<Recipient> match = o.m_recipients;
    foreach (const Recipient &r, m_recipients) {
        int i = match.indexOf(r);
        if (i < 0)
            return false;
        match.removeAt(i);
    }

    return true;
}

namespace {

QList<Recipient> removeMatches(const RecipientList &list, bool (Recipient::*mf)(const Recipient &) const)
{
    QList<Recipient> rv;
    rv.reserve(list.size());

    foreach (const Recipient &r, list) {
        QList<Recipient>::const_iterator it = rv.constBegin(), end = rv.constEnd();
        for ( ; it != end; ++it) {
            if ((r.*mf)(*it)) {
                break;
            }
        }
        if (it == end) {
            // No previous instances that match this recipient
            rv.append(r);
        }
    }

    return rv;
}

QList<Recipient> removeMatches(const RecipientList &list)
{
    return removeMatches(list, &Recipient::matches);
}

QList<Recipient> removeSameContacts(const RecipientList &list)
{
    return removeMatches(list, &Recipient::isSameContact);
}

}

bool RecipientList::matches(const RecipientList &o) const
{
    if (m_recipients.size() == 1 && o.m_recipients.size() == 1)
        return m_recipients.first().matches(o.m_recipients.first());

    QList<Recipient> myRecipients(removeMatches(m_recipients));
    QList<Recipient> otherRecipients(removeMatches(o.m_recipients));

    if (myRecipients.size() != otherRecipients.size())
        return false;

    foreach (const Recipient &r, myRecipients) {
        QList<Recipient>::iterator it = otherRecipients.begin(), end = otherRecipients.end();
        for ( ; it != end; ++it) {
            if (r.matches(*it)) {
                otherRecipients.erase(it);
                break;
            }
        }
        if (it == end)
            return false;
    }

    return true;
}

bool RecipientList::hasSameContacts(const RecipientList &o) const
{
    if (m_recipients.size() == 1 && o.m_recipients.size() == 1)
        return m_recipients.first().isSameContact(o.m_recipients.first());

    QList<Recipient> myRecipients(removeSameContacts(m_recipients));
    QList<Recipient> otherRecipients(removeSameContacts(o.m_recipients));

    if (myRecipients.size() != otherRecipients.size())
        return false;

    foreach (const Recipient &r, myRecipients) {
        QList<Recipient>::iterator it = otherRecipients.begin(), end = otherRecipients.end();
        for ( ; it != end; ++it) {
            if (r.isSameContact(*it)) {
                otherRecipients.erase(it);
                break;
            }
        }
        if (it == end)
            return false;
    }

    return true;
}

bool RecipientList::allContactsResolved() const
{
    foreach (const Recipient &r, m_recipients) {
        if (!r.isContactResolved())
            return false;
    }
    return true;
}

QString RecipientList::debugString() const
{
    QString output = QStringLiteral("(");
    foreach (const Recipient &r, m_recipients) {
        if (output.size() > 1)
            output.append(QStringLiteral(", "));

        output.append(QStringLiteral("[%1:%2]").arg(r.localUid()).arg(r.remoteUid()));
        if (r.contactId() > 0)
            output.append(QStringLiteral(" -> %1 '%2'").arg(r.contactId()).arg(r.contactName()));
    }
    output.append(QLatin1Char(')'));
    return output;
}

RecipientList::iterator RecipientList::begin()
{
    return m_recipients.begin();
}

RecipientList::iterator RecipientList::end()
{
    return m_recipients.end();
}

RecipientList::iterator RecipientList::find(const Recipient &r)
{
    iterator it = begin(), end = this->end();
    for ( ; it != end; ++it) {
        if (it->isSameContact(r)) {
            break;
        }
    }
    return it;
}

RecipientList::iterator RecipientList::findMatch(const Recipient &r)
{
    iterator it = begin(), end = this->end();
    for ( ; it != end; ++it) {
        if (it->matches(r)) {
            break;
        }
    }
    return it;
}

RecipientList::const_iterator RecipientList::constBegin() const
{
    return m_recipients.constBegin();
}

RecipientList::const_iterator RecipientList::constEnd() const
{
    return m_recipients.constEnd();
}

RecipientList::const_iterator RecipientList::constFind(const Recipient &r) const
{
    const_iterator it = constBegin(), end = constEnd();
    for ( ; it != end; ++it) {
        if (it->isSameContact(r)) {
            break;
        }
    }
    return it;
}

RecipientList::const_iterator RecipientList::constFindMatch(const Recipient &r) const
{
    const_iterator it = constBegin(), end = constEnd();
    for ( ; it != end; ++it) {
        if (it->matches(r)) {
            break;
        }
    }
    return it;
}

bool RecipientList::contains(const Recipient &r) const
{
    return (constFind(r) != constEnd());
}

bool RecipientList::containsMatch(const Recipient &r) const
{
    return (constFindMatch(r) != constEnd());
}

Recipient RecipientList::value(int index) const
{
    return m_recipients.value(index);
}

const Recipient &RecipientList::at(int index) const
{
    return m_recipients.at(index);
}

void RecipientList::append(const Recipient &r)
{
    m_recipients.append(r);
}

void RecipientList::append(const QList<Recipient> &o)
{
    m_recipients.append(o);
}

RecipientList &RecipientList::unite(const RecipientList &other)
{
    foreach (const Recipient &r, other.m_recipients) {
        if (constFind(r) == constEnd()) {
            append(r);
        }
    }
    return *this;
}

RecipientList &RecipientList::operator<<(const Recipient &recipient)
{
    if (!m_recipients.contains(recipient))
        m_recipients.append(recipient);
    return *this;
}

QDebug &operator<<(QDebug &debug, const CommHistory::Recipient &r)
{
    debug.nospace() << "Recipient(" << r.localUid() << " " << r.remoteUid()
        << " | " << r.contactId() << " " << r.contactName() << ")";
    return debug.space();
}

QDebug &operator<<(QDebug &debug, const CommHistory::RecipientList &list)
{
    debug.nospace() << "RecipientList(";
    for (CommHistory::RecipientList::const_iterator begin = list.begin(), it = begin, end = list.end(); it != end; ++it) {
        if (it != begin) {
            debug << ", ";
        }
        debug << *it;
    }
    debug << ")";
    return debug.space();
}

QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::Recipient &recipient)
{
    argument.beginStructure();
    argument << recipient.localUid() << recipient.remoteUid();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, CommHistory::Recipient &recipient)
{
    QString localUid, remoteUid;
    argument.beginStructure();
    argument >> localUid >> remoteUid;
    argument.endStructure();
    recipient = Recipient(localUid, remoteUid);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::RecipientList &recipients)
{
    argument.beginArray(qMetaTypeId<Recipient>());
    foreach (const Recipient &r, recipients.recipients())
        argument << r;
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, CommHistory::RecipientList &recipients)
{
    argument.beginArray();
    while (!argument.atEnd()) {
        Recipient r;
        argument >> r;
        recipients << r;
    }
    argument.endArray();
    return argument;
}

