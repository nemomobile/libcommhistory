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

#include <QCoreApplication>
#include <QDebug>

#include <QContactManager>
#include <QContactFetchRequest>
#include <QContactOnlineAccount>
#include <QContactDetailFilter>
#include <QContactPhoneNumber>
#include <QContactName>
#include <QContactNickname>
#include <QContactPresence>
#include <QContactDisplayLabel>
#include <QContactIntersectionFilter>

#include "commonutils.h"

#include "contactlistener.h"
#include "qcontacttpmetadata_p.h"

using namespace CommHistory;

QWeakPointer<ContactListener> ContactListener::m_Instance;

namespace {

    static const int CONTACT_REQUEST_THRESHOLD = 5000;
    static const int REQUEST_BATCH_SIZE = 10;

    QContactFilter addContactFilter(const QContactFilter &existingFilter,
                                    const QContactFilter &newFilter)
    {
        if (existingFilter == QContactFilter())
            return newFilter;

        return existingFilter | newFilter;
    }
}

#ifdef USING_QTPIM
int ContactListener::internalContactId(const QContactIdType &id)
{
    // We need to be able to represent an ID as a 32-bit int; we could use
    // hashing, but for now we will just extract the integral part of the ID
    // string produced by qtcontacts-sqlite
    if (!id.isNull()) {
        QStringList components = id.toString().split(QChar::fromLatin1(':'));
        const QString &idComponent = components.isEmpty() ? QString() : components.last();
        if (idComponent.startsWith(QString::fromLatin1("sql-"))) {
            return idComponent.mid(4).toUInt();
        }
    }
    return 0;
}

int ContactListener::internalContactId(const QContact &contact)
{
    return internalContactId(contact.id());
}

QContactIdType ContactListener::apiContactId(int iid)
{
    // Currently only works with qtcontacts-sqlite
    QContactId contactId;
    if (iid != 0) {
        static const QString idStr(QString::fromLatin1("qtcontacts:org.nemomobile.contacts.sqlite::sql-%1"));
        contactId = QContactId::fromString(idStr.arg(iid));
        if (contactId.isNull()) {
            qWarning() << "Unable to formulate valid ID from:" << iid;
        }
    }
    return contactId;
}
#else
int ContactListener::internalContactId(const QContactIdType &id)
{
    return id;
}

int ContactListener::internalContactId(const QContact &contact)
{
    return contact.localId();
}

QContactIdType ContactListener::apiContactId(int id)
{
    return id;
}
#endif

ContactListener::ContactListener(QObject *parent)
    : QObject(parent),
      m_Initialized(false)
{
}

ContactListener::~ContactListener()
{
}

QSharedPointer<ContactListener> ContactListener::instance()
{
    QSharedPointer<ContactListener> result;
    if (!m_Instance) {
        result = QSharedPointer<ContactListener>(new ContactListener());
        result->init();
        m_Instance = result.toWeakRef();
    } else {
        result = m_Instance.toStrongRef();
    }

    return result;
}

void ContactListener::init()
{
    if (m_Initialized)
        return;

    qDebug() << Q_FUNC_INFO;

    if (!m_ContactManager) {
#ifdef USING_QTPIM
        // Temporary override until qtpim supports QTCONTACTS_MANAGER_OVERRIDE
        QString envspec(QStringLiteral("org.nemomobile.contacts.sqlite"));
#else
        QString envspec(QLatin1String(qgetenv("NEMO_CONTACT_MANAGER")));
#endif
        if (!envspec.isEmpty()) {
            qDebug() << "Using contact manager:" << envspec;
            m_ContactManager = new QContactManager(envspec);
        } else {
            m_ContactManager = new QContactManager;
        }
        m_ContactManager->setParent(this);
#ifdef USING_QTPIM
        connect(m_ContactManager, SIGNAL(contactsAdded(QList<QContactId>)),
                this, SLOT(slotContactsUpdated(QList<QContactId>)));
        connect(m_ContactManager, SIGNAL(contactsChanged(QList<QContactId>)),
                this, SLOT(slotContactsUpdated(QList<QContactId>)));
        connect(m_ContactManager, SIGNAL(contactsRemoved(QList<QContactId>)),
                this, SLOT(slotContactsRemoved(QList<QContactId>)));
#else
        connect(m_ContactManager, SIGNAL(contactsAdded(QList<QContactLocalId>)),
                this, SLOT(slotContactsUpdated(QList<QContactLocalId>)));
        connect(m_ContactManager, SIGNAL(contactsChanged(QList<QContactLocalId>)),
                this, SLOT(slotContactsUpdated(QList<QContactLocalId>)));
        connect(m_ContactManager, SIGNAL(contactsRemoved(QList<QContactLocalId>)),
                this, SLOT(slotContactsRemoved(QList<QContactLocalId>)));
#endif
    }

    m_ContactTimer.setSingleShot(true);
    m_ContactTimer.setInterval(CONTACT_REQUEST_THRESHOLD);
    connect(&m_ContactTimer, SIGNAL(timeout()), this, SLOT(slotStartContactRequest()));

    m_Initialized = true;
}

QContactFetchRequest* ContactListener::buildRequest(const QContactFilter &filter)
{
    QContactFetchRequest *request = new QContactFetchRequest();
    request->setManager(m_ContactManager);
    request->setParent(this);
    request->setFilter(filter);

#ifdef USING_QTPIM
    QList<QContactDetail::DetailType> details;
    details << QContactName::Type
            << QContactOnlineAccount::Type
            << QContactPhoneNumber::Type
            << QContactNickname::Type
            << QContactPresence::Type
            << QContactDisplayLabel::Type;

    QContactFetchHint hint;
    hint.setDetailTypesHint(details);
#else
    QStringList details;
    details << QContactName::DefinitionName
            << QContactOnlineAccount::DefinitionName
            << QContactPhoneNumber::DefinitionName
            << QContactNickname::DefinitionName
            << QContactPresence::DefinitionName 
            << QContactDisplayLabel::DefinitionName;

    QContactFetchHint hint;
    hint.setDetailDefinitionsHint(details);
#endif

    // Relationships are slow and unnecessary here
    hint.setOptimizationHints(QContactFetchHint::NoRelationships);

    request->setFetchHint(hint);
    return request;
}

void ContactListener::slotContactsUpdated(const QList<QContactIdType> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO << contactIds;

    m_PendingContactIds.append(contactIds);
    startRequestOrTimer();
}

void ContactListener::slotContactsRemoved(const QList<QContactIdType> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO << contactIds;

    foreach (const QContactIdType &localId, contactIds)
        emit contactRemoved(internalContactId(localId));
}

void ContactListener::slotStartContactRequest()
{
    qDebug() << Q_FUNC_INFO;
    QContactFetchRequest *request = 0;

    if (!m_PendingUnresolvedContacts.isEmpty()) {
        QContactFilter filter;

        for (int i = 0; i < REQUEST_BATCH_SIZE && !m_PendingUnresolvedContacts.isEmpty(); i++) {
            QPair<QString,QString> contact = *m_PendingUnresolvedContacts.begin();
            m_PendingUnresolvedContacts.erase(m_PendingUnresolvedContacts.begin());

            QString number = CommHistory::normalizePhoneNumber(contact.second,
                                                               NormalizeFlagKeepDialString);

            QPair<QString,QString> address = contact;
            if (address.first.indexOf("/ring/tel/") >= 0) {
                // The resolved result will be an empty localUid
                address.first = QString();
            }
            if (!number.isEmpty()) {
                address.second = number;
            }
            m_RequestedContacts.insert(address);

            if (number.isEmpty()) {
                QContactDetailFilter filterRemote;
                filterRemote.setValue(contact.second);
                filterRemote.setMatchFlags(QContactFilter::MatchExactly);

#ifdef USING_QTPIM
                filterRemote.setDetailType(QContactOnlineAccount::Type, QContactOnlineAccount::FieldAccountUri);
#else
                filterRemote.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                                     QContactOnlineAccount::FieldAccountUri);
#endif

                QContactIntersectionFilter tpFilter;
                tpFilter << QContactTpMetadata::matchAccountId(contact.first);
                tpFilter << filterRemote;
                filter = addContactFilter(filter, tpFilter);
            } else {
                filter = addContactFilter(filter, QContactPhoneNumber::match(number));
            }
        }
        request = buildRequest(filter);
    }

    if (!request && !m_PendingContactIds.isEmpty()) {
        QList<QContactIdType> requestIds;

        for (int i = 0; i < REQUEST_BATCH_SIZE && !m_PendingContactIds.isEmpty(); i++)
            requestIds << m_PendingContactIds.takeFirst();

        QContactIdTypeFilter filter;
        filter.setIds(requestIds);
        request = buildRequest(filter);
    }

    if (request) {
        connect(request, SIGNAL(resultsAvailable()),
                this, SLOT(slotResultsAvailable()));
        request->start();
    }
}

void ContactListener::slotResultsAvailable()
{
    QContactFetchRequest *request = qobject_cast<QContactFetchRequest *>(sender());

    if (!request || !request->isFinished())
        return;

    qDebug() << Q_FUNC_INFO << request->contacts().size() << "contacts";

    foreach (const QContact &contact, request->contacts()) {
#ifdef USING_QTPIM
        if (contact.id() != m_ContactManager->selfContactId())
#else
        if (contact.localId() != m_ContactManager->selfContactId())
#endif
        {
            QList< QPair<QString,QString> > addresses;
            foreach (const QContactOnlineAccount &account, contact.details<QContactOnlineAccount>()) {
#ifdef USING_QTPIM
                QString localUid = account.value(QContactOnlineAccount__FieldAccountPath).toString();
#else
                QString localUid = account.value(QLatin1String("AccountPath"));
#endif
                addresses += qMakePair(localUid, account.accountUri());
                m_RequestedContacts.remove(addresses.last());
            }
            foreach (const QContactPhoneNumber &phoneNumber, contact.details<QContactPhoneNumber>()) {
                addresses += qMakePair(QString(), phoneNumber.number());
                m_RequestedContacts.remove(qMakePair(QString(), CommHistory::normalizePhoneNumber(phoneNumber.number(), NormalizeFlagKeepDialString)));
            }
            emit contactUpdated(internalContactId(contact), contactName(contact), addresses);
        } // if
    }

    // Report any unresolved contacts as unknown
    QSet<QPair<QString, QString> >::const_iterator it = m_RequestedContacts.constBegin(), end = m_RequestedContacts.constEnd();
    for ( ; it != end; ++it) {
        emit contactUnknown(*it);
    }
    m_RequestedContacts.clear();

    request->deleteLater();

    slotStartContactRequest();
}

bool ContactListener::addressMatchesList(const QString &localUid,
                                         const QString &remoteUid,
                                         const QList< QPair<QString,QString> > &contactAddresses)
{
    bool found = false;

    QListIterator<QPair<QString,QString> > i(contactAddresses);
    while (i.hasNext()) {
        QPair<QString,QString> address = i.next();
        if ((address.first.isEmpty() || address.first == localUid)
            && CommHistory::remoteAddressMatch(remoteUid, address.second)) {
            found = true;
            break;
        }
    }

    return found;
}

void ContactListener::resolveContact(const QString &localUid,
                                     const QString &remoteUid)
{
    qDebug() << Q_FUNC_INFO << localUid << remoteUid;

    QPair<QString, QString> unresolved(localUid, remoteUid);
    if (!m_PendingUnresolvedContacts.contains(unresolved)) {
        m_PendingUnresolvedContacts.insert(unresolved);
        startRequestOrTimer();
    }
}

void ContactListener::startRequestOrTimer()
{
    // if it's only one new contact or conversation it's probably hand-added
    // than start request right away and start timer to avoid subsequent requests if it's mass update
    if (!m_ContactTimer.isActive()
        && (m_PendingContactIds.size() + m_PendingUnresolvedContacts.size()) == 1)
        slotStartContactRequest();
    m_ContactTimer.start();
}

bool ContactListener::isLastNameFirst()
{
    return false;
}

bool ContactListener::preferNickname()
{
    return false;
}

QString ContactListener::contactName(const QContact &contact)
{
    QString displayLabel, firstName, lastName, contactNickname, imNickname;

    /* Use the CustomLabel field if present, otherwise get first/last name */
    foreach (const QContactName &name, contact.details<QContactName>()) {
        if (name.isEmpty())
            continue;

#ifdef USING_QTPIM
        displayLabel = name.value(QContactName__FieldCustomLabel).toString();
#else
        displayLabel = name.customLabel();
#endif
        if (!displayLabel.isEmpty())
            return displayLabel;

        firstName = name.firstName();
        lastName = name.lastName();
        break;
    }

    /* Use QContactDisplayLabel as a fallback */
#ifdef USING_QTPIM
    displayLabel = contact.detail<QContactDisplayLabel>().label();
#else
    displayLabel = contact.displayLabel();
#endif
    if (!displayLabel.isEmpty())
        return displayLabel;

    /* Generate the name */
    foreach (const QContactNickname &nickname, contact.details<QContactNickname>()) {
        if (!nickname.isEmpty()) {
            contactNickname = nickname.nickname();
            break;
        }
    }
    foreach (const QContactPresence &presence, contact.details<QContactPresence>()) {
        if (!presence.isEmpty() && !presence.nickname().isEmpty()) {
            imNickname = presence.nickname();
            break;
        }
    }

    QString realName;
    if (!firstName.isEmpty() || !lastName.isEmpty()) {
        QString lname;
        if (isLastNameFirst())  {
            realName = lastName;
            lname = firstName;
        } else {
            realName = firstName;
            lname = lastName;
        }

        if (!lname.isEmpty()) {
            if (!realName.isEmpty())
                realName.append(' ');
            realName.append(lname);
        }
    }

    if (preferNickname() && !contactNickname.isEmpty())
        return contactNickname;
    else if (!realName.isEmpty())
        return realName;
    else if (!imNickname.isEmpty())
        return imNickname;
    else
        return contactNickname;
}
  
