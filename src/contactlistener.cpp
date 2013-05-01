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
#include <QContactLocalIdFilter>
#include <QContactPhoneNumber>
#include <QContactId>
#include <QContactName>
#include <QContactDisplayLabel>
#include <qtcontacts-tracker/settings.h>
#include <qtcontacts-tracker/customdetails.h>

#include "commonutils.h"

#include "contactlistener.h"
#include "qcontacttpmetadata_p.h"

using namespace CommHistory;

QWeakPointer<ContactListener> ContactListener::m_Instance;

namespace {

#ifndef COMMHISTORY_USE_QTCONTACTS_API
    // Using the tracker backend for QtMobility Contacts
    static const QLatin1String CONTACT_STORAGE_TYPE("tracker");
#endif
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
#ifdef COMMHISTORY_USE_QTCONTACTS_API
        QString envspec(QLatin1String(qgetenv("NEMO_CONTACT_MANAGER")));
        if (!envspec.isEmpty()) {
            qDebug() << "Using contact manager:" << envspec;
            m_ContactManager = new QContactManager(envspec);
        } else {
            m_ContactManager = new QContactManager;
        }
#else
        QMap<QString,QString> params;
        params["contact-types"] = QLatin1String("contact");
        params["omit-presence-changes"] = QLatin1String(""); // value ignored
        m_ContactManager = new QContactManager(CONTACT_STORAGE_TYPE, params);
#endif
        m_ContactManager->setParent(this);
        connect(m_ContactManager, SIGNAL(contactsAdded(const QList<QContactLocalId> &)),
                this, SLOT(slotContactsUpdated(const QList<QContactLocalId> &)));
        connect(m_ContactManager, SIGNAL(contactsChanged(const QList<QContactLocalId> &)),
                this, SLOT(slotContactsUpdated(const QList<QContactLocalId> &)));
        connect(m_ContactManager, SIGNAL(contactsRemoved(const QList<QContactLocalId> &)),
                this, SLOT(slotContactsRemoved(const QList<QContactLocalId> &)));
    }

    if (!m_Settings) {
        m_Settings = new QctSettings(this);
        connect(m_Settings, SIGNAL(valuesChanged(const QHash<QString, QVariant> &)),
                this, SLOT(slotSettingsChanged(const QHash<QString, QVariant> &)));
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

    QStringList details;
    details << QContactName::DefinitionName
            << QContactOnlineAccount::DefinitionName
            << QContactPhoneNumber::DefinitionName
            << QContactDisplayLabel::DefinitionName;

    QContactFetchHint hint;
    hint.setDetailDefinitionsHint(details);
    request->setFetchHint(hint);

    return request;
}

void ContactListener::slotContactsUpdated(const QList<QContactLocalId> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO << contactIds;

    m_PendingContactIds.append(contactIds);
    startRequestOrTimer();
}

void ContactListener::slotContactsRemoved(const QList<QContactLocalId> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO << contactIds;

    foreach (const QContactLocalId &localId, contactIds)
        emit contactRemoved(localId);
}

void ContactListener::slotStartContactRequest()
{
    qDebug() << Q_FUNC_INFO;
    QContactFetchRequest *request = 0;

    if (!m_PendingUnresolvedContacts.isEmpty()) {
        QContactFilter filter;

        for (int i = 0; i < REQUEST_BATCH_SIZE && !m_PendingUnresolvedContacts.isEmpty(); i++) {
            QPair<QString,QString> contact = m_PendingUnresolvedContacts.takeFirst();

            QString number = CommHistory::normalizePhoneNumber(contact.second,
                                                               NormalizeFlagKeepDialString);

            if (number.isEmpty()) {
                QContactDetailFilter filterLocal;
                filterLocal.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                                    QLatin1String("AccountPath"));
                filterLocal.setValue(contact.first);

                QContactDetailFilter filterRemote;
                filterRemote.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                                     QContactOnlineAccount::FieldAccountUri);
                filterRemote.setValue(contact.second);

                filter = addContactFilter(filter, filterLocal & filterRemote);
            } else {
                filter = addContactFilter(filter, QContactPhoneNumber::match(number));
            }
        }
        request = buildRequest(filter);
    }

    if (!request && !m_PendingContactIds.isEmpty()) {
        QList<QContactLocalId> requestIds;

        for (int i = 0; i < REQUEST_BATCH_SIZE && !m_PendingContactIds.isEmpty(); i++)
            requestIds << m_PendingContactIds.takeFirst();

        QContactLocalIdFilter filter;
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
        if (contact.localId() != m_ContactManager->selfContactId()) {
            QList< QPair<QString,QString> > addresses;
            foreach (const QContactOnlineAccount &account,
                     contact.details(QContactOnlineAccount::DefinitionName)) {
                addresses += qMakePair(account.value(QLatin1String("AccountPath")),
                                       account.accountUri());
            }
            foreach (const QContactPhoneNumber &phoneNumber,
                     contact.details(QContactPhoneNumber::DefinitionName)) {
                addresses += qMakePair(QString(), phoneNumber.number());
            }

            emit contactUpdated(contact.localId(), contact.displayLabel(), addresses);
        } // if
    }

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
        m_PendingUnresolvedContacts << unresolved;
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

void ContactListener::slotSettingsChanged(const QHash<QString, QVariant> &changedSettings)
{
    qDebug() << Q_FUNC_INFO << changedSettings;

    foreach (const QString &setting, changedSettings.keys()) {
        if (setting == QctSettings::NameOrderKey
            || setting == QctSettings::PreferNicknameKey) {
            emit contactSettingsChanged(changedSettings);
            break;
        }
    }
}

bool ContactListener::isLastNameFirst()
{
    return (m_Settings->nameOrder() == QContactDisplayLabel__FieldOrderLastName);
}

bool ContactListener::preferNickname()
{
    return m_Settings->preferNickname();
}
