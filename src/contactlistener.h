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

#ifndef COMMHISTORY_CONTACTLISTENER_H
#define COMMHISTORY_CONTACTLISTENER_H

// QT includes
#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>
#include <QPointer>

#include "libcommhistoryexport.h"

// contacts
#include <QContact>
#include <QContactFetchRequest>

#ifdef USING_QTPIM
# include <QContactId>
# include <QContactIdFilter>

QTCONTACTS_USE_NAMESPACE

typedef QContactId QContactIdType;
typedef QContactIdFilter QContactIdTypeFilter;

#else
# include <QContactLocalIdFilter>

QTM_USE_NAMESPACE
QTM_BEGIN_NAMESPACE
class QContactManager;
class QContactFetchRequest;
QTM_END_NAMESPACE

typedef QContactLocalId QContactIdType;
typedef QContactLocalIdFilter QContactIdTypeFilter;

#endif

namespace CommHistory {

class LIBCOMMHISTORY_EXPORT ContactListener : public QObject
{
    Q_OBJECT

public:
    /*!
     *  \returns Contact listener
     */
    static QSharedPointer<ContactListener> instance();

    ~ContactListener();

    static bool addressMatchesList(const QString &localUid,
                                   const QString &remoteUid,
                                   const QList< QPair<QString,QString> > &contactAddresses);

    static int internalContactId(const QContactIdType &id);
    static int internalContactId(const QContact &contact);
    static QContactIdType apiContactId(int internalId);

    /**
     * Find a contact for (localUid, remoteUid), result provided via conactUpdate() signal.
     */
    void resolveContact(const QString &localUid,
                        const QString &remoteUid);

    /**
     * Get contact name from a QContact. Should have QContactName, QContactNickname,
     * and QContactPresence details. */
    QString contactName(const QContact &contact);

    /**
     * Get address book settings.
     */
    bool isLastNameFirst();
    bool preferNickname();

Q_SIGNALS:
    void contactUpdated(quint32 localId,
                        const QString &contactName,
                        const QList< QPair<QString,QString> > &contactAddresses);
    void contactRemoved(quint32 localId);

private Q_SLOTS:
#ifdef USING_QTPIM
    void slotContactsUpdated(const QList<QContactId> &contactIds);
    void slotContactsRemoved(const QList<QContactId> &contactIds);
#else
    void slotContactsUpdated(const QList<QContactLocalId> &contactIds);
    void slotContactsRemoved(const QList<QContactLocalId> &contactIds);
#endif

    void slotStartContactRequest();
    void slotResultsAvailable();

private:
    ContactListener(QObject *parent = 0);

    void init();
    QContactFetchRequest *buildRequest(const QContactFilter &filter);
    void startRequestOrTimer();

private:
    static QWeakPointer<ContactListener> m_Instance;
    bool m_Initialized;
    QTimer m_ContactTimer;
    QPointer<QContactManager> m_ContactManager;
    QList<QContactIdType> m_PendingContactIds;
    QList<QPair<QString,QString> > m_PendingUnresolvedContacts;
};

}

#endif
