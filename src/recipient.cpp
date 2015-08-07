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

namespace CommHistory {

class RecipientPrivate
{
public:
    QString localUid;
    QString remoteUid;
    QString contactName;
    int contactId;
    bool contactResolved;
    bool isPhoneNumber;

    RecipientPrivate(const QString &localUid, const QString &remoteUid);
    ~RecipientPrivate();

    static QSharedPointer<RecipientPrivate> get(const QString &localUid, const QString &remoteUid);
};

}

using namespace CommHistory;

namespace {

bool initializeTypes()
{
    qRegisterMetaType<CommHistory::Recipient>();
    qRegisterMetaType<CommHistory::RecipientList>();
    return true;
}

}

bool recipient_initialized = initializeTypes();

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
    , contactId(0)
    , contactResolved(false)
{
    isPhoneNumber = localUid.startsWith(QStringLiteral("/org/freedesktop/Telepathy/Account/ring/"));
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
    return d->contactName.isEmpty() ? d->remoteUid : d->contactName;
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
    return d->isPhoneNumber ? CommHistory::minimizePhoneNumber(d->remoteUid) : d->remoteUid;
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

    return d->localUid == o.d->localUid && CommHistory::remoteAddressMatch(d->localUid, d->remoteUid, o.d->remoteUid, true);
}

bool Recipient::matches(const QString &o) const
{
    return d->remoteUid == o || CommHistory::remoteAddressMatch(d->localUid, d->remoteUid, o, true);
}

bool Recipient::isSameContact(const Recipient &o) const
{
    if (d == o.d)
        return true;
    if (d->contactResolved && o.d->contactResolved && (d->contactId > 0 || o.d->contactId > 0))
        return d->contactId == o.d->contactId;
    return matches(o);
}

int Recipient::contactId() const
{
    return d->contactId;
}

QString Recipient::contactName() const
{
    return d->contactName;
}

bool Recipient::isContactResolved() const
{
    return d->contactResolved;
}

void Recipient::setResolvedContact(int contactId, const QString &contactName) const
{
    if (d->contactResolved && contactId == d->contactId && contactName == d->contactName)
        return;

    if (!d->contactResolved || d->contactId != contactId) {
        if (d->contactResolved)
            recipientContactMap->remove(d->contactId, d);
        recipientContactMap->insert(contactId, d.toWeakRef());
    }

    d->contactResolved = true;
    d->contactId = contactId;
    d->contactName = contactName;
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

bool RecipientList::matches(const RecipientList &o) const
{
    QSet<int> otherMatches;

    foreach (const Recipient &r, m_recipients) {
        bool found = false;
        
        /* We have to check for a match against every item in the other list,
         * so we can later tell if any of the other's items weren't a match to
         * any of ours. */
        for (int i = 0; i < o.m_recipients.size(); i++) {
            if (r.matches(o.m_recipients[i])) {
                otherMatches.insert(i);
                found = true;
            }
        }

        if (!found)
            return false;
    }

    return otherMatches.size() == o.m_recipients.size();
}

bool RecipientList::hasSameContacts(const RecipientList &o) const
{
    QSet<int> otherMatches;

    foreach (const Recipient &r, m_recipients) {
        bool found = false;

        for (int i = 0; i < o.m_recipients.size(); i++) {
            if (r.isSameContact(o.m_recipients[i])) {
                otherMatches.insert(i);
                found = true;
            }
        }

        if (!found)
            return false;
    }

    return otherMatches.size() == o.m_recipients.size();
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

