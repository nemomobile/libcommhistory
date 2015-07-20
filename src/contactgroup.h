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

#ifndef COMMHISTORY_CONTACTGROUP_H
#define COMMHISTORY_CONTACTGROUP_H

#include <QObject>

#include "libcommhistoryexport.h"
#include "groupobject.h"

namespace CommHistory {

class ContactGroupPrivate;
class ContactGroupModel;

class LIBCOMMHISTORY_EXPORT ContactGroup : public QObject
{
    Q_OBJECT

public:
    ContactGroup(QObject *parent = 0);

    /* Properties */
    Q_PROPERTY(QList<int> contactIds READ contactIds NOTIFY contactIdsChanged);
    QList<int> contactIds() const;

    Q_PROPERTY(QStringList displayNames READ displayNames NOTIFY displayNamesChanged);
    QStringList displayNames() const;

    Q_PROPERTY(QDateTime startTime READ startTime NOTIFY startTimeChanged)
    QDateTime startTime() const;

    Q_PROPERTY(QDateTime endTime READ endTime NOTIFY endTimeChanged)
    QDateTime endTime() const;

    Q_PROPERTY(int unreadMessages READ unreadMessages NOTIFY unreadMessagesChanged)
    int unreadMessages() const;

    Q_PROPERTY(int lastEventId READ lastEventId NOTIFY lastEventChanged)
    int lastEventId() const;

    Q_PROPERTY(CommHistory::GroupObject* lastEventGroup READ lastEventGroup NOTIFY lastEventChanged)
    GroupObject *lastEventGroup() const;

    Q_PROPERTY(QString lastMessageText READ lastMessageText NOTIFY lastEventChanged)
    QString lastMessageText() const;

    Q_PROPERTY(QString lastVCardFileName READ lastVCardFileName NOTIFY lastEventChanged)
    QString lastVCardFileName() const;

    Q_PROPERTY(QString lastVCardLabel READ lastVCardLabel NOTIFY lastEventChanged)
    QString lastVCardLabel() const;

    Q_PROPERTY(int lastEventType READ lastEventType NOTIFY lastEventChanged)
    int lastEventType() const;

    Q_PROPERTY(int lastEventStatus READ lastEventType NOTIFY lastEventChanged)
    int lastEventStatus() const;

    Q_PROPERTY(bool lastEventIsDraft READ lastEventIsDraft NOTIFY lastEventChanged)
    bool lastEventIsDraft() const;

    Q_PROPERTY(QDateTime lastModified READ lastModified NOTIFY lastModifiedChanged)
    QDateTime lastModified() const;
 
    Q_PROPERTY(QList<QObject*> groups READ groupObjects NOTIFY groupsChanged)
    QList<GroupObject*> groups() const;
    QList<QObject*> groupObjects() const;

    quint32 startTimeT() const;
    quint32 endTimeT() const;
    quint32 lastModifiedT() const;

    bool isResolved() const;
    void resolve();

    Q_INVOKABLE CommHistory::GroupObject *findGroup(const QString &localUid, const QString &remoteUid);
    Q_INVOKABLE CommHistory::GroupObject *findGroup(const QString &localUid, const QStringList &remoteUids);

public slots:
    void addGroup(CommHistory::GroupObject *group);
    bool removeGroup(CommHistory::GroupObject *group);
    void updateGroup(CommHistory::GroupObject *group);

    bool markAsRead();
    bool deleteGroups();

signals:
    void contactIdsChanged();
    void displayNamesChanged();
    void startTimeChanged();
    void endTimeChanged();
    void unreadMessagesChanged();
    void lastEventChanged();
    void lastModifiedChanged();
    void groupsChanged();

private:
    Q_DECLARE_PRIVATE(ContactGroup)
    ContactGroupPrivate *d_ptr;
};

}

Q_DECLARE_METATYPE(CommHistory::ContactGroup*)

#endif
