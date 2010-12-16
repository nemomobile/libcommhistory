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

#include <QtDBus/QtDBus>
#include <QtTracker/Tracker>
#include <QDebug>
#include <QtTracker/ontologies/nmo.h>

#include "trackerio.h"
#include "eventmodel.h"
#include "eventmodel_p.h"
#include "unreadeventsmodel.h"
#include "event.h"

using namespace SopranoLive;

namespace CommHistory {

using namespace CommHistory;

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

    reset();
    d->clearEvents();

    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::Message>();
    message.property<nmo::isRead>(LiteralValue(false));
    message.property<nmo::isDraft>(LiteralValue(false));
    message.property<nmo::isDeleted>(LiteralValue(false));
    if(!includeSentEvents) {
        message.property<nmo::isSent>(LiteralValue(false));
    }

    d->tracker()->prepareMessageQuery(query, message, d->propertyMask);

    return d->executeQuery(query);
}

} // namespace CommHistory
