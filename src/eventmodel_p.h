/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jolla.com>
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

#ifndef COMMHISTORY_EVENTMODEL_P_H
#define COMMHISTORY_EVENTMODEL_P_H

#include <QList>
#include <QGenericArgument>

#include "eventmodel.h"
#include "event.h"
#include "eventtreeitem.h"
#include "databaseio.h"
#include "libcommhistoryexport.h"
#include "contactlistener.h"
#include "contactresolver.h"

class QSqlQuery;

namespace CommHistory {

class UpdatesEmitter;

/*!
 * \class EventModelPrivate
 *
 * Contains most of the implementation for EventModel. Inheritable
 * for submodels.
 */
class LIBCOMMHISTORY_EXPORT EventModelPrivate : public QObject
{
    Q_OBJECT

    Q_DECLARE_PUBLIC(EventModel)

public:
    EventModel *q_ptr;

    EventModelPrivate(EventModel *model = 0);
    ~EventModelPrivate();

    /*!
     * Implement this method in submodels. acceptsEvent() must return
     * true if the given event fits the current model (for example, the
     * event type is correct or the group id matches).
     *
     * \param event Event to compare against the model criteria.
     * \return true if can be shown in the model.
     */
    virtual bool acceptsEvent(const Event &event) const;

    /*!
     * Tries to find the event with the specified id in the internal
     * tree storage.
     *
     * \param id Event id.
     * \return model index of the event (valid if found).
     */
    virtual QModelIndex findEvent(int id) const;

    /*!
     * Executes a database query. fillModel() is called when new events
     * are received, and modelReady() is emitted when the query is
     * finished.
     */
    bool executeQuery(QSqlQuery &query);

    /*!
     * Add new events from the query results to the internal event
     * structure. You can reimplement this for non-trivial models, such
     * as trees.
     *
     * \param start Start row for new events in the internal tracker
     * query model.
     * \param end End row for new events in the the internal tracker
     * query model.
     * \param events List of new events to be inserted into the model.
     *
     * \return true if successful (return value not used at the moment).
     */
    virtual bool fillModel(int start, int end, QList<CommHistory::Event> events, bool resolved);

    /*!
     * Delete all events from the internal event storage.
     */
    virtual void clearEvents();

    void addToModel(const Event &event, bool synchronous = false) { addToModel(QList<Event>() << event, synchronous); }

    virtual void addToModel(const QList<Event> &event, bool synchronous = false);
    virtual void modifyInModel(Event &event);
    virtual void deleteFromModel(int id);

    QModelIndex findEventRecursive(int id, EventTreeItem *parent) const;

    bool canFetchMore() const;

    void setResolveContacts(EventModel::ContactResolveType resolveType);
    void resolveAddedEvents(const QList<Event> &events);

    void resolveIfRequired(const Event &event) const;

    DatabaseIO *database();

    void recipientsChangedRecursive(const QSet<Recipient> &recipients, EventTreeItem *parent);
    void emitDataChanged(int row, void *data);

    // This is the root node for the internal event tree. In a standard
    // flat model, eventRootNode has rowCount() children with events.
    // Use this in fillModel() and other methods if you're implementing
    // a nonstandard model.
    EventTreeItem *eventRootItem;

    mutable ContactResolver *addResolver, *receiveResolver, *onDemandResolver;
    mutable QList<Event> pendingAdded, pendingReceived, pendingOnDemand;

    bool isInTreeMode;
    EventModel::QueryMode queryMode;
    uint chunkSize;
    uint firstChunkSize;
    int queryLimit;
    int queryOffset;
    bool isReady;
    bool threadCanFetchMore;

    // Do not set directly, use setResolveContacts to enable listener
    EventModel::ContactResolveType resolveContacts;

    Event::PropertySet propertyMask;

    QSharedPointer<ContactListener> contactListener;

    QThread *bgThread;

    QSharedPointer<UpdatesEmitter> emitter;

public Q_SLOTS:
    virtual void prependEvents(QList<Event> events, bool resolved);
    virtual bool fillModel(QList<Event> events, bool resolved);

    virtual void receiveResolverFinished();
    virtual void addResolverFinished();
    virtual void onDemandResolverFinished();

    virtual void eventsReceivedSlot(int start, int end, QList<CommHistory::Event> events);

    virtual void modelUpdatedSlot(bool successful);

    virtual void eventsAddedSlot(const QList<CommHistory::Event> &events);

    virtual void eventsUpdatedSlot(const QList<CommHistory::Event> &events);

    virtual void eventDeletedSlot(int id);

    virtual void canFetchMoreChangedSlot(bool canFetch);

    virtual void slotContactInfoChanged(const RecipientList &recipients);

    virtual void slotContactChanged(const RecipientList &recipients);

Q_SIGNALS:
    void eventsAdded(const QList<CommHistory::Event> &events);

    void eventsUpdated(const QList<CommHistory::Event> &events);

    void eventDeleted(int id);

    void groupsUpdated(const QList<int> &groupIds);
    void groupsUpdatedFull(const QList<CommHistory::Group> &groups);

    void groupsDeleted(const QList<int> &groupIds);

    void modelReady(bool successful);
    void eventsCommitted(const QList<CommHistory::Event> &events, bool successful);
};

}

#endif
