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

#ifndef COMMHISTORY_GROUPMODEL_P_H
#define COMMHISTORY_GROUPMODEL_P_H

#include <QAbstractItemModel>
#include <QList>
#include <QPair>

#include "groupmodel.h"
#include "eventmodel.h"
#include "groupmanager.h"

namespace CommHistory {

class GroupModelPrivate: public QObject
{
    Q_OBJECT

    Q_DECLARE_PUBLIC(GroupModel);

public:
    GroupModel *q_ptr;

    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    GroupModelPrivate(GroupModel *parent = 0);
    ~GroupModelPrivate();

    void setManager(GroupManager *manager);
    void ensureManager();

    GroupManager *manager;
    QList<GroupObject*> groups;

public slots:
    void groupAdded(GroupObject *group);
    void groupUpdated(GroupObject *group);
    void groupDeleted(GroupObject *group);
};

}

#endif
