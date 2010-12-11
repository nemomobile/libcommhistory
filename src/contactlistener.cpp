/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
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

#include "commonutils.h"

#include "contactlistener.h"

using namespace CommHistory;

QWeakPointer<ContactListener> ContactListener::m_Instance;

namespace {
    static const QLatin1String CONTACT_STORAGE_TYPE("tracker");
    static const int CONTACT_REQUEST_THRESHOLD = 5000;
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
        m_ContactManager = new QContactManager(CONTACT_STORAGE_TYPE);
        m_ContactManager->setParent(this);
        connect(m_ContactManager, SIGNAL(contactsAdded(const QList<QContactLocalId> &)),
                this, SLOT(slotContactsUpdated(const QList<QContactLocalId> &)));
        connect(m_ContactManager, SIGNAL(contactsChanged(const QList<QContactLocalId> &)),
                this, SLOT(slotContactsUpdated(const QList<QContactLocalId> &)));
        connect(m_ContactManager, SIGNAL(contactsRemoved(const QList<QContactLocalId> &)),
                this, SLOT(slotContactsRemoved(const QList<QContactLocalId> &)));
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
            << QContactPhoneNumber::DefinitionName;

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
    m_ContactTimer.start();
}

void ContactListener::slotContactsRemoved(const QList<QContactLocalId> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO << contactIds;

    foreach (QContactLocalId localId, contactIds)
        emit contactRemoved(localId);
}

void ContactListener::slotStartContactRequest()
{
    QContactLocalIdFilter filter;
    filter.setIds(m_PendingContactIds);

    QContactFetchRequest *request = buildRequest(filter);
    connect(request, SIGNAL(resultsAvailable()),
            this, SLOT(slotResultsAvailable()));

    request->start();
}

void ContactListener::slotResultsAvailable()
{
    QContactFetchRequest *request = qobject_cast<QContactFetchRequest *>(sender());

    if (!request || !request->isFinished())
        return;

    qDebug() << Q_FUNC_INFO << request->contacts().size() << "contacts";

    foreach (QContact contact, request->contacts()) {
        QList< QPair<QString,QString> > addresses;
        foreach (QContactOnlineAccount account,
                 contact.details(QContactOnlineAccount::DefinitionName)) {
            addresses += qMakePair(account.value(QLatin1String("AccountPath")),
                                   account.accountUri());
        }
        foreach (QContactPhoneNumber phoneNumber,
                 contact.details(QContactPhoneNumber::DefinitionName)) {
            addresses += qMakePair(QString(), phoneNumber.number());
        }

        emit contactUpdated(contact.localId(), contact.displayLabel(), addresses);
    }

    request->deleteLater();
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
    QContactFilter filter;

    QString number = CommHistory::normalizePhoneNumber(remoteUid);
    if (number.isEmpty()) {
        QContactDetailFilter filterLocal;
        filterLocal.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                            QLatin1String("AccountPath"));
        filterLocal.setValue(localUid);

        QContactDetailFilter filterRemote;
        filterRemote.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                             QContactOnlineAccount::FieldAccountUri);
        filterRemote.setValue(remoteUid);

        filter = filterLocal & filterRemote;
    } else {
        filter = QContactPhoneNumber::match(remoteUid);
    }

    QContactFetchRequest *request = buildRequest(filter);
    connect(request, SIGNAL(resultsAvailable()),
            this, SLOT(slotResultsAvailable()));

    request->start();
}
