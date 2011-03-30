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
#include <QDebug>

#include "trackerio.h"
#include "trackerio_p.h"
#include "eventmodel.h"
#include "eventmodel_p.h"
#include "callmodel.h"
#include "callmodel_p.h"
#include "event.h"
#include "commonutils.h"
#include "contactlistener.h"
#include "eventsquery.h"
#include "queryrunner.h"
#include "updatequery.h"
#include "committingtransaction.h"

namespace {
    static CommHistory::Event::PropertySet unusedProperties = CommHistory::Event::PropertySet()
        << CommHistory::Event::IsDraft
        << CommHistory::Event::FreeText
        << CommHistory::Event::MessageToken
        << CommHistory::Event::ReportDelivery
        << CommHistory::Event::ValidityPeriod
        << CommHistory::Event::ContentLocation
        << CommHistory::Event::FromVCardFileName
        << CommHistory::Event::FromVCardLabel
        << CommHistory::Event::MessageParts
        << CommHistory::Event::Cc
        << CommHistory::Event::Bcc
        << CommHistory::Event::ReadStatus
        << CommHistory::Event::ReportRead
        << CommHistory::Event::ReportReadRequested
        << CommHistory::Event::MmsId
        << CommHistory::Event::To;
}

namespace CommHistory
{

using namespace CommHistory;

/* ************************************************************************** *
 * ******** P R I V A T E   C L A S S   I M P L E M E N T A T I O N ********* *
 * ************************************************************************** */

CallModelPrivate::CallModelPrivate( EventModel *model )
        : EventModelPrivate( model )
        , sortBy( CallModel::SortByContact )
        , eventType( CallEvent::UnknownCallType )
        , referenceTime( QDateTime() )
        , hasBeenFetched( false )
{
    contactChangesEnabled = true;
    propertyMask -= unusedProperties;
    connect(this, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&,bool)),
            this, SLOT(slotEventsCommitted(const QList<CommHistory::Event>&,bool)));
    connect(partQueryRunner, SIGNAL(resultsReceived(QSparqlResult *)),
            this, SLOT(doDeleteCallGroup(QSparqlResult *)));
}

void CallModelPrivate::executeGroupedQuery(const QString &query)
{
    qDebug() << __PRETTY_FUNCTION__;

    isReady = false;
    if (queryMode == EventModel::StreamedAsyncQuery) {
        queryRunner->setStreamedMode(true);
        queryRunner->setChunkSize(chunkSize);
        queryRunner->setFirstChunkSize(firstChunkSize);
    }
    queryRunner->runGroupedCallQuery(query);
    if (queryMode == EventModel::SyncQuery) {
        QEventLoop loop;
        while (!isReady || !messagePartsReady) {
            loop.processEvents(QEventLoop::WaitForMoreEvents);
        }
    }
}

bool CallModelPrivate::acceptsEvent( const Event &event ) const
{
    qDebug() << __PRETTY_FUNCTION__ << event.id();
    if ( event.type() != Event::CallEvent )
    {
        return false;
    }

    if(!referenceTime.isNull() && (event.startTime() < referenceTime)) // a reference Time is already set, so any further event addition should be beyond that
    {
        return false;
    }

    if(this->eventType != CallEvent::UnknownCallType)
    {
        if(eventType == CallEvent::MissedCallType && !(event.direction() == Event::Inbound && event.isMissedCall()))
        {
                return false;
        }
        else if(eventType == CallEvent::DialedCallType && !(event.direction() == Event::Outbound) )
        {
            return false;
        }
        else if(eventType == CallEvent::ReceivedCallType && !(event.direction() == Event::Inbound && !event.isMissedCall()))
        {
            return false;
        }
    }

    return true;
}

bool CallModelPrivate::belongToSameGroup( const Event &e1, const Event &e2 )
{
    if (sortBy == CallModel::SortByContact
        && remoteAddressMatch(e1.remoteUid(), e2.remoteUid(), NormalizeFlagKeepDialString)
        && e1.localUid() == e2.localUid())
    {
        return true;
    }
    else if (sortBy == CallModel::SortByTime
             && (remoteAddressMatch(e1.remoteUid(), e2.remoteUid(), NormalizeFlagKeepDialString)
                 && e1.localUid() == e2.localUid()
                 && e1.direction() == e2.direction()
                 && e1.isMissedCall() == e2.isMissedCall()))
    {
        return true;
    }
    return false;
}

int CallModelPrivate::calculateEventCount( EventTreeItem *item )
{
    int count = -1;

    switch ( sortBy )
    {
        case CallModel::SortByContact :
        {
            // set event count for missed calls only,
            // leave the default value for non-missed ones
            if ( item->event().isMissedCall() )
            {
                count = 1;
                // start looping the list from index number 1, because
                // the index number 0 is the same item as the top level
                // one
                for ( int i = 1; i < item->childCount(); i++ )
                {
                    if ( item->child( i - 1 )->event().isMissedCall() &&
                         item->child( i )->event().isMissedCall() )
                    {
                        count++;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            break;
        }
        case CallModel::SortByTime :
        {
            count = item->childCount();
            break;
        }
        default:
            break;
    }

    return count;
}

bool CallModelPrivate::fillModel( int start, int end, QList<CommHistory::Event> events )
{
    Q_UNUSED( start );
    Q_UNUSED( end );
    Q_Q( CallModel );

    //for flat mode EventModelPrivate::fillModel is sufficient as all the events will be stored at the top level
    if(!isInTreeMode)
    {
        return EventModelPrivate::fillModel(start, end, events);
    }

    if ( events.count() > 0 )
    {
        /*
         * call events are grouped as follows:
         *
         * [event 1] - (event 1)
         *             (event 2)
         *             (event 3)
         * [event 4] - (event 4)
         *             (event 5)
         * ...         ...
         *
         * NOTE:
         * on the top level, there are only the representatives of each group
         * on second level, there are all call events listed (also group reps)
         */

        QList<EventTreeItem *> topLevelItems;
        // get the first event and save it as top level item
        Event event = events.first();
        topLevelItems.append(new EventTreeItem(event));

        switch ( sortBy )
        {
            /*
             * if sorted by contact,
             * then event count is meaningful only for missed calls.
             * it shows how many missed calls there are under the top
             * level one without breaking the continuity of the missed
             * calls.
             *
             * John, 3 missed
             * Mary, 1 received
             * John, 1 dialed
             * John, 1 missed
             * Mary, 2 dialed
             *
             *      ||
             *      \/
             *
             * John, 3 missed
             * Mary, received
             *
             * NOTE 1:
             * there are actually 4 missed calls from John, but there was
             * 1 dialed in between, that is why the event count is 3.
             *
             * NOTE 2:
             * there is no number for the received calls, since only the
             * missed calls have valid even count. (But -1 will be returned.)
             */
            case CallModel::SortByContact :
            {
                QList<CommHistory::Event> newEvents;
                for (int i = 1; i < events.count(); i++) {
                    Event event = events.at(i);
                    if (event.contactId() > 0) {
                        contactCache.insert(qMakePair(event.localUid(), event.remoteUid()),
                                            qMakePair(event.contactId(), event.contactName()));
                    }

                    bool found = false;
                    for (int i = 0; i < eventRootItem->childCount() && !found; i++) {
                        // ignore matching events because the already existing
                        // entry has to be more recent
                        if (belongToSameGroup(eventRootItem->child(i)->event(), event))
                            found = true;
                    }

                    if (!found)
                        topLevelItems.append(new EventTreeItem(event));
                }
                break;
            }
            /*
             * if sorted by time,
             * then event count is the number of events grouped under the
             * top level one
             *
             * John, 3 missed
             * Mary, 1 received
             * John, 1 dialed
             * John, 1 missed
             * Mary, 2 dialed
             */
            case CallModel::SortByTime  :
            {
                // add first item to the top and also to the second level
                topLevelItems.last()->appendChild(new EventTreeItem(event, topLevelItems.last()));

                // loop through the result set
                for ( int row = 1; row < events.count(); row++ )
                {
                    Event event = events.at( row );
                    // if event is NOT groupable with the last top level one, then create new group
                    if ( !belongToSameGroup( event, topLevelItems.last()->event() ) )
                    {
                        topLevelItems.append( new EventTreeItem( event ) );
                    }
                    // add event to the last group
                    // this is an existing or a freshly created one with the same event as representative
                    topLevelItems.last()->appendChild( new EventTreeItem( event, topLevelItems.last() ) );
                }

                // once the events are grouped,
                // loop through the top level items and update event counts
                foreach ( EventTreeItem *item, topLevelItems )
                {
                    item->event().setEventCount( calculateEventCount( item ) );
                }

                break;
            }

            default:
                break;
        }

        // save top level items into the model
        q->beginInsertRows( QModelIndex(), 0, topLevelItems.count() - 1);
        foreach ( EventTreeItem *item, topLevelItems )
        {
            eventRootItem->appendChild( item );
        }
        q->endInsertRows();
    }

    return true;
}

void CallModelPrivate::addToModel( Event &event )
{
    Q_Q(CallModel);
    qDebug() << __PRETTY_FUNCTION__ << event.id();

    if(!isInTreeMode)
    {
            return EventModelPrivate::addToModel(event);
    }

    if (event.contactId() > 0) {
        contactCache.insert(qMakePair(event.localUid(), event.remoteUid()),
                            qMakePair(event.contactId(), event.contactName()));
    } else {
        if (!setContactFromCache(event)) {
            // calls don't have the luxury of only one contact per
            // conversation -> resolve unknowns and add to cache
            startContactListening();
            if (contactListener)
                contactListener->resolveContact(event.localUid(), event.remoteUid());
        }
    }

    switch ( sortBy )
    {
        case CallModel::SortByContact :
        {
            // find match, update count if needed, move to top
            int matchingRow = -1;
            for (int i = 0; i < eventRootItem->childCount(); i++) {
                if (belongToSameGroup(eventRootItem->child(i)->event(), event)) {
                    matchingRow = i;
                    break;
                }
            }

            if (matchingRow != -1) {
                EventTreeItem *matchingItem = eventRootItem->child(matchingRow);
                // replace with new event
                int eventCount = matchingItem->event().eventCount();
                if (eventCount < 0)
                    eventCount = 0;
                event.setEventCount(event.isMissedCall() ? eventCount + 1 : 0);
                matchingItem->setEvent(event);

                // move to top if not there already
                if (matchingRow != 0) {
                    emit q->layoutAboutToBeChanged();
                    eventRootItem->moveChild(matchingRow, 0);
                    emit q->layoutChanged();
                } else {
                    emit q->dataChanged(q->createIndex(0, 0, eventRootItem->child(0)),
                                        q->createIndex(0, CallModel::NumberOfColumns - 1,
                                                       eventRootItem->child(0)));
                }
            } else {
                emit q->beginInsertRows(QModelIndex(), 0, 0);
                event.setEventCount(0);
                eventRootItem->prependChild(new EventTreeItem(event));
                emit q->endInsertRows();
            }

            break;
        }
        case CallModel::SortByTime :
        {
            // if new item is groupable with the first one in the list
            // NOTE: assumption is that time value is ok
            if ( eventRootItem->childCount() && belongToSameGroup( event, eventRootItem->child( 0 )->event() ) )
            {
                // alias
                EventTreeItem *firstTopLevelItem = eventRootItem->child( 0 );
                // add event to the group, set it as top level item and refresh event count
                firstTopLevelItem->prependChild( new EventTreeItem( event, firstTopLevelItem ) );
                firstTopLevelItem->setEvent( event );
                firstTopLevelItem->event().setEventCount( calculateEventCount( firstTopLevelItem ) );
                // only counter and timestamp of first must be updated
                emit q->dataChanged( q->createIndex( 0, 0, eventRootItem->child( 0 ) ),
                                     q->createIndex( 0, CallModel::NumberOfColumns - 1, eventRootItem->child( 0 ) ) );
            }
            // create a new group, otherwise
            else
            {
                // a new row must be inserted
                q->beginInsertRows( QModelIndex(), 0, 0 );
                // add new item as first on the list
                eventRootItem->prependChild( new EventTreeItem( event ) );
                // alias
                EventTreeItem *firstTopLevelItem = eventRootItem->child( 0 );
                // add the copy of the event to its local list and refresh event count
                firstTopLevelItem->prependChild( new EventTreeItem( event, firstTopLevelItem ) );
                firstTopLevelItem->event().setEventCount( calculateEventCount( firstTopLevelItem ) );
                q->endInsertRows();
            }
            break;
        }
        default :
        {
            qWarning() << __PRETTY_FUNCTION__ << "Adding call events to model sorted by type or by service has not been implemented yet.";
            return;
        }
    }
}

void CallModelPrivate::eventsAddedSlot( const QList<Event> &events )
{
    qDebug() << __PRETTY_FUNCTION__ << events.count();
    // TODO: sorting?
    EventModelPrivate::eventsAddedSlot(events);
}

void CallModelPrivate::eventsUpdatedSlot( const QList<Event> &events )
{
    // TODO regrouping of events might occur =(
    qWarning() << __PRETTY_FUNCTION__ << "Specific behaviour has not been implemented yet.";
    EventModelPrivate::eventsUpdatedSlot( events );
}

QModelIndex CallModelPrivate::findEvent( int id ) const
{
    Q_Q( const CallModel );

    if(!isInTreeMode)
    {
        return EventModelPrivate::findEvent(id);
    }

    for ( int row = 0; row < eventRootItem->childCount(); row++ )
    {
        // check top level item
        if ( eventRootItem->child( row )->event().id() == id )
        {
            return q->createIndex( row, 0, eventRootItem->child( row ) );
        }
        // loop through all grouped events
        EventTreeItem *currentGroup = eventRootItem->child( row );
        for ( int column = 0; column < currentGroup->childCount(); column++ )
        {
            if ( currentGroup->child( column )->event().id() == id )
            {
                return q->createIndex( row, column, currentGroup->child( column ) );
            }
        }
    }

    // id was not found, return invalid index
    return QModelIndex();
}

void CallModelPrivate::deleteFromModel( int id )
{
    Q_Q(CallModel);

    if(!isInTreeMode)
    {
        return EventModelPrivate::deleteFromModel(id);
    }

    // TODO : what if an event is deleted from the db through commhistory-tool?

    // seek for the top level item which was deleted
    QModelIndex index = findEvent( id );

    // if id was not found, do nothing
    if ( !index.isValid() )
    {
        qDebug() << __PRETTY_FUNCTION__ << "*** Invalid";
        return;
    }

    // TODO : it works only when sorting is time based

    // if event is a top level item ( i.e. the whole group ), then delete it
    if ( index.column() == 0 )
    {
        int row = index.row();
        bool isRegroupingNeeded = false;
        // regrouping is needed/possible only if sorting is SortByTime...
        // ...and there is a previous row and a following row to group together
        if ( sortBy == CallModel::SortByTime &&
             row - 1 >= 0 && row + 1 < eventRootItem->childCount() )
        {
            EventTreeItem *prev = eventRootItem->child( row - 1 );
            EventTreeItem *next = eventRootItem->child( row + 1 );

            if ( belongToSameGroup( prev->event(), next->event() ) )
            {
                for ( int i = 0; i < next->childCount(); i++ )
                {
                    prev->appendChild( new EventTreeItem( next->child( i )->event() ) );
                }
                prev->event().setEventCount( calculateEventCount( prev ) );
                isRegroupingNeeded = true;
            }
        }

        qDebug() << __PRETTY_FUNCTION__ << "*** Top level" << row;
        // if there is no need to regroup the previous and following items,
        // then delete only one row
        if ( !isRegroupingNeeded )
        {
            q->beginRemoveRows( index.parent(), row, row );
            eventRootItem->removeAt( row );
        }
        // otherwise delete the current and the following one
        // (since we added content of the following to the previous)
        else
        {
            q->beginRemoveRows( index.parent(), row, row + 1 );
            eventRootItem->removeAt( row + 1 );
            eventRootItem->removeAt( row );
            emit q->dataChanged( q->createIndex( row - 1, 0, eventRootItem->child( row - 1 ) ),
                                 q->createIndex( row - 1, 0, eventRootItem->child( row - 1 ) ) );
        }
        q->endRemoveRows();
    }
    // otherwise item is a grouped event
    else
    {
        qDebug() << __PRETTY_FUNCTION__ << "*** Sth else";
        // TODO :
        // delete it from the model
        // update top level item
        // emit dataChanged()
    }
}

void CallModelPrivate::deleteCallGroup( const Event &event )
{
    qDebug() << Q_FUNC_INFO << event.id();

    // the calls could be deleted simply with "delete ?call where ?call
    // belongs to ?channel", but then we wouldn't be able to send
    // separate eventDeleted signals :(

    QSparqlQuery query(
        QLatin1String("SELECT ?call WHERE { "
                      "?call nmo:communicationChannel ?:channel . "
                      "}"));

    QUrl channelUri(TrackerIOPrivate::makeCallGroupURI(event));

    query.bindValue(QLatin1String("channel"), channelUri);

    partQueryRunner->runQuery(query);
    partQueryRunner->startQueue();
}

void CallModelPrivate::doDeleteCallGroup(QSparqlResult *result)
{
    qDebug() << Q_FUNC_INFO;

    QList<int> eventIds;
    tracker()->transaction();
    while (result->next()) {
        QString eventUri = result->value(0).toString();
        int id = Event::urlToId(eventUri);

        Event event;
        event.setType(Event::CallEvent);
        event.setId(id);
        tracker()->deleteEvent(event);
        eventIds << id;
    }

    if (eventIds.size()) {
        CommittingTransaction *t = tracker()->commit();
        if (t) {
            foreach (int id, eventIds) {
                t->addSignal(false, this,
                             "eventDeleted",
                             Q_ARG(int, id));
            }
        }
    } else {
        tracker()->rollback();
    }

    result->deleteLater();
}

void CallModelPrivate::slotEventsCommitted(const QList<CommHistory::Event> &events, bool success)
{
    Q_UNUSED(events);
    Q_Q(CallModel);

    // Empty events list means all events have been deleted (with deleteAll)
    if (success && events.isEmpty()) {
        qWarning() << __PRETTY_FUNCTION__ << "clearing model";
        q->beginResetModel();
        clearEvents();
        q->endResetModel();
    }
}

/* ************************************************************************** *
 * ********* P U B L I C   C L A S S   I M P L E M E N T A T I O N ********** *
 * ************************************************************************** */

CallModel::CallModel(QObject *parent)
        : EventModel(*new CallModelPrivate(this), parent)
{
    Q_D(CallModel);
    d->isInTreeMode = true;
}

CallModel::CallModel(CallModel::Sorting sorting, QObject* parent = 0)
        : EventModel(*new CallModelPrivate(this), parent)
{
    Q_D( CallModel );
    d->isInTreeMode = true;

    setFilter( sorting );
}

CallModel::~CallModel()
{
}


void CallModel::setQueryMode( EventModel::QueryMode mode )
{
    if (mode == EventModel::StreamedAsyncQuery) {
        qWarning() << __PRETTY_FUNCTION__ << "CallModel can not use streamed query mode.";
    } else {
        EventModel::setQueryMode(mode);
    }
}

bool CallModel::setFilter(CallModel::Sorting sortBy,
                          CallEvent::CallType type,
                          const QDateTime &referenceTime)
{
    Q_D(CallModel);

    // save sorting, reference Time and call event Type for filtering call events
    d->sortBy = sortBy;
    d->eventType = type;
    d->referenceTime = referenceTime;

    if ( d->hasBeenFetched )
    {
        return getEvents();
    }
    return true;
}

bool CallModel::getEvents()
{
    Q_D(CallModel);

    d->hasBeenFetched = true;

    beginResetModel();
    d->clearEvents();
    endResetModel();

    if (d->sortBy == SortByContact) {
        QString query = TrackerIOPrivate::prepareGroupedCallQuery();
        d->executeGroupedQuery(query);
        return true;
    }

    EventsQuery query(d->propertyMask);
    query.addPattern(QLatin1String("%1 a nmo:Call .")).variable(Event::Id);

    if (d->eventType != CallEvent::UnknownCallType) {
        if (d->eventType == CallEvent::ReceivedCallType) {
            query.addPattern(QLatin1String("%1 nmo:isSent \"false\" . "
                                           "%1 nmo:isAnswered \"true\". "))
                .variable(Event::Id);
        } else if (d->eventType == CallEvent::MissedCallType) {
            query.addPattern(QLatin1String("%1 nmo:isSent \"false\" . "
                                           "%1 nmo:isAnswered \"false\". "))
                .variable(Event::Id);
        } else {
            query.addPattern(QLatin1String("%1 nmo:isSent \"true\" ."))
                .variable(Event::Id);
        }

        if (!d->referenceTime.isNull()) {
            query.addPattern(QString(QLatin1String("FILTER (nmo:sentDate(%2) >= \"%1Z\"^^xsd:dateTime)"))
                             .arg(d->referenceTime.toUTC().toString(Qt::ISODate)))
                .variable(Event::Id);
        }
    }

    query.addModifier("ORDER BY DESC(%1) DESC(tracker:id(%2))")
                     .variable(Event::StartTime)
                     .variable(Event::Id);

    return d->executeQuery(query);
}

bool CallModel::getEvents(CallModel::Sorting sortBy,
                          CallEvent::CallType type,
                          const QDateTime &referenceTime)
{
    Q_D(CallModel);

    d->hasBeenFetched = true;

    return setFilter( sortBy, type, referenceTime );
}

bool CallModel::deleteAll()
{
    Q_D(CallModel);

    d->tracker()->transaction();

    bool deleted;
    deleted = d->tracker()->deleteAllEvents(Event::CallEvent);
    if (!deleted) {
        qWarning() << __PRETTY_FUNCTION__ << "Failed to delete events";
        d->tracker()->rollback();
        return false;
    }

    d->commitTransaction(QList<Event>());

    return true;
}

bool CallModel::markAllRead()
{
    Q_D(CallModel);

    d->tracker()->transaction();

    bool marked;
    marked = d->tracker()->markAsReadAll(Event::CallEvent);
    if (!marked) {
        qWarning() << __PRETTY_FUNCTION__ << "Failed to delete events";
        d->tracker()->rollback();
        return false;
    }

    d->commitTransaction(QList<Event>());

    return true;
}

bool CallModel::addEvent( Event &event )
{
    return EventModel::addEvent(event);
}

bool CallModel::modifyEvent( Event &event )
{
    qWarning() << __PRETTY_FUNCTION__ << "Specific behaviour has not been implemented yet.";
    return EventModel::modifyEvent( event );
}

bool CallModel::deleteEvent( int id )
{
    Q_D(CallModel);

    if(!d->isInTreeMode)
    {
        return EventModel::deleteEvent(id);
    }

    qDebug() << Q_FUNC_INFO << id;
    QModelIndex index = d->findEvent(id);
    if (!index.isValid())
        return false;

    switch ( d->sortBy )
    {
        case SortByContact :
        {
            EventTreeItem *item = d->eventRootItem->child(index.row());
            d->deleteCallGroup(item->event());
            return true;
        }

        case SortByTime :
        {
            EventTreeItem *item = d->eventRootItem->child( index.row() );

            d->tracker()->transaction( d->syncOnCommit );

            QList<Event> deletedEvents;

            // get all events stored in the item and delete them one by one
            for ( int i = 0; i < item->childCount(); i++ )
            {
                // NOTE: when events are sorted by time, the tree hierarchy is only 2 levels deep
                if (!d->tracker()->deleteEvent(item->child(i)->event())) {
                    d->tracker()->rollback();
                    return false;
                }
                deletedEvents << item->child( i )->event();
            }

            d->commitTransaction(deletedEvents);
            // delete event from model (not only from db)
            d->deleteFromModel( id );
            // signal delete in case someone else needs to know it
            emit d->eventDeleted( id );

            return true;
        }
        default :
        {
            qWarning() << __PRETTY_FUNCTION__ << "Deleting of call events from model sorted by type or by service has not been implemented yet.";
            return false;
        }
    }
}

bool CallModel::deleteEvent( Event &event )
{
    return deleteEvent( event.id() );
}

}
