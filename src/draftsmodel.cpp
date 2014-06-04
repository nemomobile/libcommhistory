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

#include "draftsmodel_p.h"
#include "constants.h"
#include "databaseio_p.h"
#include "commhistorydatabase.h"
#include "debug.h"
#include <QSqlQuery>

namespace CommHistory {

DraftsModelPrivate::DraftsModelPrivate(DraftsModel *model)
    : EventModelPrivate(model)
{
}

bool DraftsModelPrivate::acceptsEvent(const Event &event) const
{
    if (!event.isDraft())
        return false;
    if (!filterGroups.isEmpty() && !filterGroups.contains(event.groupId()))
        return false;
    return true;
}

DraftsModel::DraftsModel(QObject *parent)
    : EventModel(*new DraftsModelPrivate(this), parent)
{
}

DraftsModel::~DraftsModel()
{
}

QList<int> DraftsModel::filterGroups() const
{
    Q_D(const DraftsModel);
    return d->filterGroups.toList();
}

void DraftsModel::setFilterGroups(const QList<int> &list)
{
    Q_D(DraftsModel);
    QSet<int> groupIds = list.toSet();
    if (groupIds == d->filterGroups)
        return;

    d->filterGroups = groupIds;
    emit filterGroupsChanged();
}

void DraftsModel::setFilterGroup(int groupId)
{
    setFilterGroups(QList<int>() << groupId);
}

void DraftsModel::clearFilterGroups()
{
    setFilterGroups(QList<int>());
}

bool DraftsModel::getEvents()
{
    Q_D(DraftsModel);

    beginResetModel();
    d->clearEvents();
    endResetModel();

    // As in ConversationModel, a UNION ALL is used to get better
    // optimization out of sqlite
    QList<int> groups = d->filterGroups.toList();
    int unionCount = 0;
    QString q;
    do {
        if (unionCount)
            q += "UNION ALL ";
        q += DatabaseIOPrivate::eventQueryBase();
        q += "WHERE Events.isDraft = 1 ";
        
        if (unionCount < groups.size())
            q += "AND Events.groupId = " + QString::number(groups[unionCount]) + " ";

        unionCount++;
    } while (unionCount < groups.size());

    q += "ORDER BY Events.endTime DESC, Events.id DESC";

    QSqlQuery query = CommHistoryDatabase::prepare(q.toLatin1(), DatabaseIOPrivate::instance()->connection());
    return d->executeQuery(query);
}

}

