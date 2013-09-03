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

#ifndef COMMHISTORY_CONTACTGROUPMODEL_P_H
#define COMMHISTORY_CONTACTGROUPMODEL_P_H

#include "libcommhistoryexport.h"
#include "contactgroupmodel.h"
#include <QObject>
#include <QDateTime>

namespace CommHistory {

class GroupManager;
class GroupObject;
class ContactGroup;

class ContactGroupModelPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactGroupModel)

public:
    ContactGroupModel *q_ptr;

    ContactGroupModelPrivate(ContactGroupModel *parent);
    virtual ~ContactGroupModelPrivate();

    GroupManager *manager;
    QList<ContactGroup*> items;

    void setManager(GroupManager *manager);

    int indexForContacts(GroupObject *group);
    int indexForObject(GroupObject *group);

private slots:
    void groupAdded(GroupObject *group);
    void groupUpdated(GroupObject *group);
    void groupDeleted(GroupObject *group);

private:
    void itemDataChanged(int index);
};

}

#endif
