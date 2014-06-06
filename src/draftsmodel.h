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

#ifndef COMMHISTORY_DRAFTSMODEL_H
#define COMMHISTORY_DRAFTSMODEL_H

#include "eventmodel.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class DraftsModelPrivate;

class LIBCOMMHISTORY_EXPORT DraftsModel : public EventModel
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(DraftsModel)

public:
    DraftsModel(QObject *parent = 0);
    ~DraftsModel();

    Q_PROPERTY(QList<int> filterGroups READ filterGroups WRITE setFilterGroups RESET clearFilterGroups NOTIFY filterGroupsChanged)
    QList<int> filterGroups() const;
    void setFilterGroups(const QList<int> &groupIds);
    void setFilterGroup(int groupId);
    void clearFilterGroups();

    Q_INVOKABLE bool getEvents();

signals:
    void filterGroupsChanged();
};

}

#endif

