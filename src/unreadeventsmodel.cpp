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
#include "eventsquery.h"

#include "unreadeventsmodel.h"


namespace CommHistory {

class UnreadEventsModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(UnreadEventsModel);

    UnreadEventsModelPrivate(EventModel *model)
        : EventModelPrivate(model) {
    }

    bool acceptsEvent(const Event &event) const {
        if( (event.type() == Event::IMEvent) ||
            (event.type() == Event::SMSEvent) ||
            (event.type() == Event::CallEvent && event.isMissedCall())) {
            return true;
        }

        return false;
    }

    void eventsUpdatedSlot(const QList<Event> &events) {
        Q_Q(UnreadEventsModel);

        EventModelPrivate::eventsUpdatedSlot(events);
        foreach (Event event, events) {
            if(event.isRead() && acceptsEvent(event)) {
                QModelIndex index = findEvent(event.id());
                if (index.isValid()) {
                    q->beginRemoveRows(index.parent(), index.row(), index.row());
                    EventTreeItem *parent = static_cast<EventTreeItem *>(index.parent().internalPointer());
                    if (!parent) parent = eventRootItem;
                    parent->removeAt(index.row());
                    q->endRemoveRows();
                }
            }
        }
    }
};

UnreadEventsModel::UnreadEventsModel(QObject *parent)
    : EventModel(*new UnreadEventsModelPrivate(this), parent)
{
}

UnreadEventsModel::~UnreadEventsModel()
{
}

bool UnreadEventsModel::addEvent(Event& event)
{
    Q_D(UnreadEventsModel);

    if (d->acceptsEvent(event) && !event.isRead()) {
        return EventModel::addEvent(event);
    }

    return false;
}

bool UnreadEventsModel::getEvents(bool includeSentEvents)
{
    Q_D(UnreadEventsModel);

    beginResetModel();
    d->clearEvents();
    endResetModel();

    EventsQuery query(d->propertyMask);

    query.addPattern(QLatin1String("%1 nmo:isRead \"false\"; "
                                      "nmo:isDraft \"false\"; "
                                      "nmo:isDeleted \"false\" . "))
            .variable(Event::Id);

    if(!includeSentEvents)
        query.addPattern(QLatin1String("%1 nmo:isSent \"false\" . ")).variable(Event::Id);

    return d->executeQuery(query);
}

} // namespace CommHistory
