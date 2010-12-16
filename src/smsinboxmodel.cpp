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
#include "smsinboxmodel.h"
#include "event.h"

using namespace SopranoLive;

namespace CommHistory {

using namespace CommHistory;

class SMSInboxModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(SMSInboxModel);

    SMSInboxModelPrivate(EventModel *model)
        : EventModelPrivate(model) {
    }

    bool acceptsEvent(const Event &event) const {
        qDebug() << __PRETTY_FUNCTION__ << event.id();
        if (event.type() == Event::SMSEvent &&
            event.direction() == Event::Inbound) return true;

        return false;
    }
};

SMSInboxModel::SMSInboxModel(QObject *parent)
        : EventModel(*new SMSInboxModelPrivate(this), parent)
{
}

SMSInboxModel::~SMSInboxModel()
{
}

bool SMSInboxModel::getEvents()
{
    Q_D(SMSInboxModel);

    reset();
    d->clearEvents();

    RDFSelect query;
    RDFVariable message = RDFVariable::fromType<nmo::SMSMessage>();
    message.property<nmo::isSent>(LiteralValue(false));
    message.property<nmo::isDraft>(LiteralValue(false));
    message.property<nmo::isDeleted>(LiteralValue(false));

    d->tracker()->prepareMessageQuery(query, message, d->propertyMask);

    return d->executeQuery(query);
}

}
