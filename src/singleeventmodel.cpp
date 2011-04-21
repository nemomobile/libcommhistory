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
#include "group.h"
#include "eventsquery.h"

#include "singleeventmodel.h"

namespace CommHistory {

using namespace CommHistory;

class SingleEventModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(SingleEventModel);

    SingleEventModelPrivate(EventModel *model)
        : EventModelPrivate(model) {
        queryLimit = 1;
    }

    bool acceptsEvent(const Event &event) const {
        Q_UNUSED(event);
        return false;
    }
};

SingleEventModel::SingleEventModel(QObject *parent)
    : EventModel(*new SingleEventModelPrivate(this), parent)
{
}

SingleEventModel::~SingleEventModel()
{
}

bool SingleEventModel::getEventByUri(const QUrl &uri)
{
    Q_D(SingleEventModel);

    reset();
    d->clearEvents();

    EventsQuery query(d->propertyMask);

    query.addPattern(QString(QLatin1String("FILTER(%2 = <%1>) ")).arg(uri.toString()))
            .variable(Event::Id);

    return d->executeQuery(query);
}

bool SingleEventModel::getEventByTokens(const QString &token,
                                        const QString &mmsId,
                                        int groupId)
{
    Q_D(SingleEventModel);

    reset();
    d->clearEvents();

    EventsQuery query(d->propertyMask);

    QStringList pattern;
    if (!token.isEmpty()) {
        pattern << QString(QLatin1String("{ %2 nmo:messageId \"%1\" }")).arg(token);
    }
    if (!mmsId.isEmpty()) {
        pattern << QString(QLatin1String("{ %2 nmo:mmsId \"%1\"; nmo:isSent true }")).arg(mmsId);
    }

    query.addPattern(pattern.join(QLatin1String("UNION"))).variable(Event::Id);
    if (groupId > -1)
        query.addPattern(QString(QLatin1String("%2 nmo:communicationChannel <%1> ."))
                         .arg(Group::idToUrl(groupId).toString()))
        .variable(Event::Id);

    query.setDistinct(true);

    return d->executeQuery(query);
}

} // namespace CommHistory
