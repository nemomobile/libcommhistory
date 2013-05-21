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

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
# include <QContactLocalIdFilter>

QTM_USE_NAMESPACE
QTM_BEGIN_NAMESPACE
class QContactManager;
class QContactFetchRequest;
QTM_END_NAMESPACE

#else
# include <QContactId>
# include <QContactIdFilter>
# define QContactLocalId QContactId
# define QContactLocalIdFilter QContactIdFilter

using namespace QtContacts;
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

    /**
     * Find a contact for (localUid, remoteUid), result provided via conactUpdate() signal.
     */
    void resolveContact(const QString &localUid,
                        const QString &remoteUid);

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
    void slotContactsUpdated(const QList<QContactLocalId> &contactIds);
    void slotContactsRemoved(const QList<QContactLocalId> &contactIds);
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
    QList<QContactLocalId> m_PendingContactIds;
    QList<QPair<QString,QString> > m_PendingUnresolvedContacts;
};

}

#endif
