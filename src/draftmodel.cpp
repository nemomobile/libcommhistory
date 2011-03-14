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

#include <QDebug>

#include "eventmodel_p.h"
#include "draftmodel.h"
#include "eventsquery.h"

namespace CommHistory {

using namespace CommHistory;

class DraftModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(DraftModel);

    DraftModelPrivate(EventModel *model)
            : EventModelPrivate(model) {
    }

    bool acceptsEvent(const Event &event) const {
        qDebug() << __PRETTY_FUNCTION__ << event.id();
        return event.isDraft();
    }
};

DraftModel::DraftModel(QObject *parent)
        : EventModel(*new DraftModelPrivate(this), parent)
{
}

DraftModel::~DraftModel()
{
}

bool DraftModel::getEvents()
{
    Q_D(DraftModel);

    reset();
    d->clearEvents();

    EventsQuery query(d->propertyMask);

    query.addPattern(QLatin1String("%1 nmo:isDraft \"true\"; nmo:isDeleted \"false\" ."))
            .variable(Event::Id);

    return d->executeQuery(query);
}

}
