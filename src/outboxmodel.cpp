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

#include <QDebug>

#include "eventmodel_p.h"
#include "outboxmodel.h"
#include "eventsquery.h"

namespace CommHistory {

using namespace CommHistory;

class OutboxModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(OutboxModel);

    OutboxModelPrivate(EventModel *model)
            : EventModelPrivate(model) {
    }

    bool acceptsEvent(const Event &event) const {
        qDebug() << __PRETTY_FUNCTION__ << event.id();
        if ((event.type() == Event::IMEvent
             || event.type() == Event::SMSEvent
             || event.type() == Event::MMSEvent) &&
            !event.isDraft() && event.direction() == Event::Outbound)
        {
            return true;
        }

        return false;
    }
};

OutboxModel::OutboxModel(QObject *parent)
        : EventModel(*new OutboxModelPrivate(this), parent)
{
}

OutboxModel::~OutboxModel()
{
}

bool OutboxModel::getEvents()
{
    Q_D(OutboxModel);

    reset();
    d->clearEvents();

    EventsQuery query(d->propertyMask);

    query.addPattern(QLatin1String("%1 nmo:isSent \"true\"; "
                                      "nmo:isDraft \"false\"; "
                                      "nmo:isDeleted \"false\" ."))
            .variable(Event::Id);

    return d->executeQuery(query);

}

}
