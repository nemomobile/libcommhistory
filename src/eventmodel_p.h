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

#ifndef COMMHISTORY_EVENTMODEL_P_H
#define COMMHISTORY_EVENTMODEL_P_H

#include <QList>
#include <QSqlError>
#include <QGenericArgument>

#include <QtTracker/Tracker>

#include "eventmodel.h"
#include "event.h"
#include "messagepart.h"
#include "eventtreeitem.h"
#include "trackerio.h"
#include "libcommhistoryexport.h"
#include "committingtransaction.h"

namespace CommHistory {

class QueryRunner;
class ContactListener;

/*!
 * \class EventModelPrivate
 *
 * Contains most of the implementation for EventModel. Inheritable
 * for submodels.
 */
class LIBCOMMHISTORY_EXPORT EventModelPrivate : public QObject
{
    Q_OBJECT

    Q_DECLARE_PUBLIC(EventModel);

    typedef enum {
        ContactUpdated, ContactRemoved
    } ContactChangeType;

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
     * Tries to find a suitable parent for the given new event.
     * Reimplement for tree models.
     *
     * \param event Event to be added.
     * \return model parent index for the event. If invalid, event will
     * not be added.
     */
    virtual QModelIndex findParent(const Event &event);

    /*!
     * Executes a tracker query. fillModel() is called when new events
     * are received, and modelReady() is emitted when the query is
     * finished.
     */
    virtual bool executeQuery(SopranoLive::RDFSelect &query);

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
    virtual bool fillModel(int start, int end, QList<CommHistory::Event> events);

    /*!
     * Delete all events from the internal event storage.
     */
    virtual void clearEvents();

    virtual void addToModel(Event &event);
    virtual void modifyInModel(Event &event);
    virtual void deleteFromModel(int id);

    virtual bool doAddEvent(Event &event);
    virtual bool doDeleteEvent(int id, Event &event);

    QString newObjectPath();
    QModelIndex findEventRecursive(int id, EventTreeItem *parent) const;

    CommittingTransaction& commitTransaction(const QList<Event> &events);

    bool canFetchMore() const;

    /*
     * Called when contacts are somehow modified. Traverses through the
     * event list and updates event.contactId() and event.contactName()
     * as necessary. Also updates contact cache.
     * \param changeType Contact change type (removed, updated (= added or modified)).
     * \param contactId LocalId of the modified contact.
     * \param contactName Name of the modified contact. Empty for removed contacts.
     * \param contactAddresses List of all IM addresses and phone numbers for the contact. Empty for removed contacts.
     * \param parent Current top item for recursion (start with eventRootItem).
     */
    void changeContactsRecursive(ContactChangeType changeType,
                                 quint32 contactId,
                                 const QString &contactName,
                                 const QList< QPair<QString,QString> > &contactAddresses,
                                 EventTreeItem *parent);

    void resetQueryRunners();
    void deleteQueryRunners();

    TrackerIO *tracker();

    // This is the root node for the internal event tree. In a standard
    // flat model, eventRootNode has rowCount() children with events.
    // Use this in fillModel() and other methods if you're implementing
    // a nonstandard model.
    EventTreeItem *eventRootItem;

    bool isInTreeMode;
    EventModel::QueryMode queryMode;
    uint chunkSize;
    uint firstChunkSize;
    int queryLimit;
    int queryOffset;
    bool isReady;
    bool messagePartsReady;
    bool threadCanFetchMore;
    bool syncOnCommit;
    bool contactChangesEnabled;

    QueryRunner *queryRunner;
    QueryRunner *partQueryRunner;
    SopranoLive::LiveNodes queryResult;

    static uint modelSerial;
    QSqlError lastError;

    Event::PropertySet propertyMask;

    QSharedPointer<ContactListener> contactListener;

    // remote id -> contact id and name
    // TODO: contact should be defined by <localId, remoteId>, at least for IM
    QMap<QString, QPair<int, QString> > contactCache;

    QList<CommittingTransaction> transactions;

    QThread *bgThread;

    TrackerIO *m_pTracker;

public Q_SLOTS:
    virtual void eventsReceivedSlot(int start, int end, QList<CommHistory::Event> events);

    virtual void messagePartsReceivedSlot(int eventId, QList<CommHistory::MessagePart> parts);

    virtual void modelUpdatedSlot();

    virtual void partsUpdatedSlot();

    virtual void eventsAddedSlot(const QList<CommHistory::Event> &events);

    virtual void eventsUpdatedSlot(const QList<CommHistory::Event> &events);

    virtual void eventDeletedSlot(int id);

    void commitFinishedSlot();
    void commitErrorSlot(QString message);

    void canFetchMoreChangedSlot(bool canFetch);

    void slotContactUpdated(quint32 localId,
                            const QString &contactName,
                            const QList< QPair<QString,QString> > &contactAddresses);

    void slotContactRemoved(quint32 localId);

Q_SIGNALS:
    void eventsAdded(const QList<CommHistory::Event> &events);

    void eventsUpdated(const QList<CommHistory::Event> &events);

    void eventDeleted(int id);

    void groupsUpdated(const QList<int> &groupIds);
    void groupsUpdatedFull(const QList<CommHistory::Group> &groups);

    void groupsDeleted(const QList<int> &groupIds);

    void modelReady();
    void eventsCommitted(const QList<CommHistory::Event> &events, bool successful);

};

}

#endif
