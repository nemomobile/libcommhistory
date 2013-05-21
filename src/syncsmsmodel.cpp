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
#include <QStringList>
#include <QSparqlResult>

#include "eventmodel_p.h"
#include "trackerio.h"
#include "eventsquery.h"
#include "committingtransaction.h"

#include "syncsmsmodel.h"
#include "syncsmsmodel_p.h"

#define MAX_ADD_EVENTS_SIZE 25

namespace CommHistory
{

using namespace CommHistory;

SyncSMSModelPrivate::SyncSMSModelPrivate(EventModel* model,
                                         int _parentId,
                                         QDateTime _dtTime,
                                         bool _lastModified)
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
    properties += Event::MessageToken;
    properties += Event::LastModified;
    properties += Event::Encoding;
    properties += Event::CharacterSet;
    properties += Event::Language;
    propertyMask = properties;
}

bool SyncSMSModelPrivate::acceptsEvent(const Event &event) const
{
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

/*
 * Callback for addEvents() transaction.
 */
void SyncSMSModelPrivate::checkTokens(CommittingTransaction *transaction,
                                      QSparqlResult *result,
                                      QVariant arg)
{
    qDebug() << Q_FUNC_INFO;

    QList<Event> events = arg.value<QList<CommHistory::Event> >();

    transaction->addSignal(true,
                           this,
                           "eventsCommitted",
                           Q_ARG(QList<CommHistory::Event>, events),
                           Q_ARG(bool, false));

    if (result && result->first()) {
        qDebug() << Q_FUNC_INFO << "tokens found";
        transaction->abort();
        return;
    }

    // no duplicate message tokens found, go ahead and add events
    for (int i = 0; i < events.count(); i++) {
        if (!doAddEvent(events[i])) {
            transaction->abort();
            return;
        }
    }

    foreach (Event event, events) {
        if (acceptsEvent(event)) {
            addToModel(event);
        }
    }

    transaction->addSignal(false,
                           this,
                           "eventsCommitted",
                           Q_ARG(QList<CommHistory::Event>, events),
                           Q_ARG(bool, true));

    transaction->addSignal(false,
                           this,
                           "eventsAdded",
                           Q_ARG(QList<CommHistory::Event>, events));
}

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

    beginResetModel();
    d->clearEvents();
    endResetModel();

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

bool SyncSMSModel::addEvents(QList<Event> &events, bool toModelOnly)
{
    Q_D(SyncSMSModel);

    qDebug() << Q_FUNC_INFO << events.count() << toModelOnly;

    if (toModelOnly) {
        // not really needed for SyncSMSModel, but let's be nice
        return EventModel::addEvents(events, toModelOnly);
    }

    QString checkDuplicatesQuery =
        QString(QLatin1String("SELECT ?msg { ?msg a nmo:Message; nmo:messageId ?id . "
                              "FILTER(?id IN (%1)) }"));

    QMutableListIterator<Event> i(events);
    QList<Event> added;
    QStringList tokens;
    while (i.hasNext()) {
        d->tracker()->transaction(d->syncOnCommit);

        // first, gather message tokens and execute the duplicate check
        // query in the transaction
        while (added.size() < MAX_ADD_EVENTS_SIZE && i.hasNext()) {
            Event &event = i.next();
            added.append(event);
            if (!event.messageToken().isEmpty())
                tokens.append(QString("\"") + event.messageToken() + QString("\""));
        }

        QSparqlQuery tokenQuery(checkDuplicatesQuery.arg(tokens.join(QChar(','))));

        // if the query returns any matches, the callback will abort the
        // transaction
        d->tracker()->currentTransaction()->addQuery(tokenQuery,
                                                     d,
                                                     "checkTokens",
                                                     QVariant::fromValue(added));

        // not using commitTransaction(), because we don't have the
        // event ids yet and can't add the eventsCommitted/Added signals
        d->tracker()->commit();

        added.clear();
        tokens.clear();
    }

    return true;
}

}
