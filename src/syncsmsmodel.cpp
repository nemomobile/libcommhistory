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
#include "trackerio.h"
#include "eventsquery.h"

#include "syncsmsmodel.h"

namespace CommHistory
{

    class SyncSMSModelPrivate: public EventModelPrivate
    {
    public:
        SyncSMSModelPrivate(EventModel* model, int _parentId, QDateTime _dtTime, bool _lastModified)
            : EventModelPrivate(model)
            , dtTime(_dtTime)
            , parentId(_parentId)
            , lastModified(_lastModified)
        {
            Event::PropertySet properties;
            properties += Event::Id;
            properties += Event::Type;
            properties += Event::StartTime;
            properties += Event::EndTime;
            properties += Event::Direction;
            properties += Event::IsDraft;
            properties += Event::IsRead;
            properties += Event::LocalUid;
            properties += Event::RemoteUid;
            properties += Event::ParentId;
            properties += Event::FreeText;
            properties += Event::GroupId;
            properties += Event::LastModified;
            properties += Event::Encoding;
            properties += Event::CharacterSet;
            properties += Event::Language;
            propertyMask = properties;
        }

        bool acceptsEvent(const Event &event) const {
            qDebug() << __PRETTY_FUNCTION__ << event.id();

            if (event.type() != Event::SMSEvent)
                return false;

            if (!lastModified) {
                if (!dtTime.isNull()) {
                    return (event.endTime() > dtTime);
                } else {
                    return true;
                }
            }

            if (!dtTime.isNull() && ((event.endTime() > dtTime)
                || (event.lastModified() <= dtTime))) {
                return false;
            } else if (lastModified && (event.lastModified() <= dtTime)) {
                return false;
            }

            return true;
        }

        QDateTime dtTime;
        int parentId;
        bool lastModified;
    };

}

using namespace CommHistory;

SyncSMSModel::SyncSMSModel(int parentId , QDateTime time, bool lastModified ,QObject *parent)
    : EventModel(*(new SyncSMSModelPrivate(this, parentId, time, lastModified)), parent)
{

}

SyncSMSModel::~SyncSMSModel()
{
}

bool SyncSMSModel::getEvents()
{
    Q_D(SyncSMSModel);

    reset();
    d->clearEvents();

    EventsQuery query(d->propertyMask);

    if (d->parentId != ALL) {
        query.addPattern(QString(QLatin1String("%2 nmo:phoneMessageId \"%1\" . "))
                         .arg(d->parentId))
                .variable(Event::Id);
    }

    if (d->lastModified) {
        if (!d->dtTime.isNull()) { //get all last modified messages after time t1
            query.addPattern(QString(QLatin1String("FILTER(tracker:added(%2) <= \"%1\"^^xsd:dateTime)"))
                             .arg(d->dtTime.toUTC().toString(Qt::ISODate)))
                    .variable(Event::Id);
            query.addPattern(QString(QLatin1String("FILTER(nie:contentLastModified(%2) > \"%1\"^^xsd:dateTime)"))
                             .arg(d->dtTime.toUTC().toString(Qt::ISODate)))
                    .variable(Event::Id);
        } else {
            query.addPattern(QString(QLatin1String("FILTER(nie:contentLastModified(%2) > \"%1\"^^xsd:dateTime)"))
                             .arg(QDateTime::fromTime_t(0).toUTC().toString(Qt::ISODate)))
                    .variable(Event::Id);
        }
    } else {
        if (!d->dtTime.isNull()) { //get all messages after time t1(including modified)
            query.addPattern(QString(QLatin1String("FILTER(tracker:added(%2) > \"%1\"^^xsd:dateTime)"))
                             .arg(d->dtTime.toUTC().toString(Qt::ISODate)))
                    .variable(Event::Id);
        }
    }

    query.addPattern(QLatin1String("%1 rdf:type nmo:SMSMessage ."))
            .variable(Event::Id);

    return d->executeQuery(query);
}

void SyncSMSModel::setSyncSmsFilter(const SyncSMSFilter& filter)
{
    Q_D(SyncSMSModel);
    d->parentId = filter.parentId;
    d->dtTime = filter.time;
    d->lastModified = filter.lastModified;
}
