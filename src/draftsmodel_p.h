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

#ifndef COMMHISTORY_DRAFTSMODEL_P_H
#define COMMHISTORY_DRAFTSMODEL_P_H

#include "eventmodel_p.h"
#include "draftsmodel.h"
#include <QSet>

namespace CommHistory {

class DraftsModelPrivate : public EventModelPrivate
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(DraftsModel)

    QSet<int> filterGroups;

    DraftsModelPrivate(DraftsModel *model);

    bool acceptsEvent(const Event &event) const;
};

}

#endif
