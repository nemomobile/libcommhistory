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
#include "singleeventmodel.h"
#include "event.h"

using namespace SopranoLive;

namespace CommHistory {

using namespace CommHistory;

class SingleEventModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(SingleEventModel);

    SingleEventModelPrivate(EventModel *model)
        : EventModelPrivate(model) {
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

    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::Message>();
    message == uri;
    d->tracker()->prepareMessageQuery(query, message, Event::allProperties());

    return d->executeQuery(query);
}

bool SingleEventModel::getEventByTokens(const QString &token,
                                        const QString &mmsId,
                                        int groupId)
{
    Q_D(SingleEventModel);

    reset();
    d->clearEvents();

    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::Message>();

    RDFPattern pattern = message.pattern().child();
    if (!token.isEmpty()) {
        pattern.variable(message).property<nmo::messageId>() = LiteralValue(token);
    }
    if (!mmsId.isEmpty()) {
        pattern = pattern.union_();
        pattern.variable(message).property<nmo::mmsId>() = LiteralValue(mmsId);
    }

    if (groupId > -1)
        message.property<nmo::communicationChannel>(Group::idToUrl(groupId));

    d->tracker()->prepareMessageQuery(query, message, Event::allProperties());

    query.distinct();

    return d->executeQuery(query);
}

} // namespace CommHistory
