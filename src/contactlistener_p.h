/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd.
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

#ifndef COMMHISTORY_CONTACTLISTENER_P_H
#define COMMHISTORY_CONTACTLISTENER_P_H

#include "contactlistener.h"

#include <QObject>
#include <QTimer>
#include <QPointer>
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

class ContactListenerPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactListener)

public:
    typedef ContactListener::ContactAddress ContactAddress;

    template<typename T1, typename T2, typename T3>
    ContactAddress makeContactAddress(T1 localUid, T2 remoteUid, T3 type) {
        ContactAddress addr;
        addr.localUid = localUid;
        addr.remoteUid = remoteUid;
        addr.type = type;
        return addr;
    }

    ContactListenerPrivate(ContactListener *q);

    QTimer m_ContactTimer;
    QPointer<QContactManager> m_ContactManager;
    QList<QContactIdType> m_PendingContactIds;
    QSet<QPair<QString,QString> > m_PendingUnresolvedContacts;
    QSet<QPair<QString,QString> > m_RequestedContacts;

    /**
     * Get contact name from a QContact. Should have QContactName, QContactNickname,
     * and QContactPresence details. */
    QString contactName(const QContact &contact);

    QContactFetchRequest *buildRequest(const QContactFilter &filter);
    void startRequestOrTimer();

public slots:
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
    ContactListener *q_ptr;
};

}

#endif
