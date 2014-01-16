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

#ifndef COMMHISTORY_EVENTMODEL_H
#define COMMHISTORY_EVENTMODEL_H

#include <QAbstractItemModel>

#include "event.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class EventModelPrivate;
class Group;
class DatabaseIO;

/*!
 * \class EventModel
 *
 * Base class for the commhistory models. Contains common utility
 * methods for accessing the commhistory database. Applications
 * generally want to use one of the subclasses instead, but the base
 * model can be used for simple operations on events.
 *
 * On simple operations on events
 *
 * Simple operations on events includes the following:
 *  * add event;
 *  * modify event;
 *  * delete event.
 * All operations modify underlying storage and emit signals
 * to sync models in the current and other processes. addEvent() will
 * send the signal right away, while other methods emit it only
 * on a sucessful commit to the database.
 */
class LIBCOMMHISTORY_EXPORT EventModel: public QAbstractItemModel
{
    Q_OBJECT

    Q_ENUMS(QueryMode)

    Q_PROPERTY(bool treeMode READ isTree WRITE setTreeMode)
    Q_PROPERTY(QueryMode queryMode READ queryMode WRITE setQueryMode)
    Q_PROPERTY(uint chunkSize READ chunkSize WRITE setChunkSize)
    Q_PROPERTY(uint firstChunkSize READ firstChunkSize WRITE setFirstChunkSize)
    Q_PROPERTY(int limit READ limit WRITE setLimit)
    Q_PROPERTY(int offset READ offset WRITE setOffset)
    Q_PROPERTY(bool ready READ isReady NOTIFY modelReady)

public:
    enum QueryMode { AsyncQuery, StreamedAsyncQuery, SyncQuery };

    enum ColumnId {
        EventId = 0,
        EventType,
        StartTime,
        EndTime,
        Direction,
        IsDraft,
        IsRead,
        IsMissedCall,
        Status,
        BytesReceived, // not implemented
        LocalUid,
        RemoteUid,
        Contacts,
        FreeText,
        GroupId,
        MessageToken,
        LastModified,
        EventCount,    // CallEvent related
        FromVCardFileName,
        FromVCardLabel,
        NumberOfColumns
    };

    enum {
        EventRole = Qt::UserRole,
        ContactIdsRole,
        ContactNamesRole,
        BaseRole = Qt::UserRole + 1000,
    };

    /*!
     * Model constructor.
     *
     * \param parent Parent object.
     */
    EventModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~EventModel();

    /*!
     * Set properties that will be fetched in getEvents() or other
     * queries by submodels. Full event data is queried by default. A
     * reduced property set will lead to faster queries, so you are
     * encouraged to use only the properties you really want.
     * The property mask will not mean that _only_ the specified
     * properties are read; for example, id and type are always valid.
     * getEvent() will always fetch the full event data.
     *
     * \param properties QSet of event properties to fetch (see Event::Property).
     */
    void setPropertyMask(const Event::PropertySet &properties);

    /*!
     * Convenience method for getting the event data without QVariant casts.
     *
     * \param index Model index.
     * \return event
     */
    Event event(const QModelIndex &index) const;

    /*!
     * Find an existing event from the model. No database queries are
     * performed.
     *
     * \param id Event id.
     * \return index of the event, invalid if not found. Use
     * index.data(Qt::UserRole).value<Event>() or model.event(index) to
     * get the event data.
     */
    virtual QModelIndex findEvent(int id) const;

    /*!
     * Set the model to tree mode or flat mode, if supported
     * (implementation left to subclasses).
     *
     * When in tree mode, root indexes contain model-specific headers
     * and events are grouped in branches under the root indexes.
     *
     * When in flat mode, the model acts as a simple list model with all
     * events at the root level.
     *
     * \param isTree true = set model to tree mode, false = set flat mode.
     */
    virtual void setTreeMode(bool isTree = true);

    /*!
     * Set query mode.
     *
     * AsyncQuery (default): The getEvents() call returns immediately,
     * and results will be fetched in the background. modelReady() is
     * emitted when all results have been received.
     *
     * StreamedAsyncQuery (not implemented yet): Same as AsyncQuery, but
     * only one chunk is fetched at a time. Use the standard Qt model
     * canFetchMore() and fetchMore() to fetch more events.
     *
     * SyncQuery: getEvents() blocks until all results have been fetched.
     *
     * \param mode Query mode.
     */
    virtual void setQueryMode(QueryMode mode);

    /*!
     * Set chunk size (number of events to fetch) for asynchronous and
     * streamed queries.
     *
     * \param size Chunk size.
     */
    virtual void setChunkSize(uint size);

    /*!
     * Set the size of first chunk (number of events to fetch) for asynchronous
     * and streamed queries.
     *
     * \param size First chunk size.
     */
    void setFirstChunkSize(uint size);

    /*!
     * Set number of events to fetch in the next query.
     *
     * \param limit Query limit.
     */
    virtual void setLimit(int limit);

    /*!
     * Set offset for the next query.
     *
     * \param offset Query offset.
     */
    virtual void setOffset(int offset);

    /*!
     * If enabled, contacts will be resolved for all events, and changes
     * to contacts will be updated live (emitting dataChanged()). Contacts
     * will be resolved before events are inserted into the model, and the
     * modelReady() signal indicates that all events are inserted and all
     * contacts are resolved.
     *
     * \param enabled If true, resolve and update contacts for events
     */
    void setResolveContacts(bool enabled);
    bool resolveContacts() const;

    // Deprecated name for setResolveContacts
    void enableContactChanges(bool enabled) { setResolveContacts(enabled); }

    /*!
     * Add a new event.
     *
     * \param event Event data to be inserted into the database. If successful,
     * event.id() is updated.
     * \param toModelOnly Optional parameter. If set to true, event is not
     * saved to database, only added to the model.
     * \return true if successful
     */
    virtual bool addEvent(Event &event, bool toModelOnly = false);

    /*!
     * Add new events.
     *
     * \param events Events to be inserted into the database. If successful,
     * event ids are updated.
     * \param toModelOnly Optional parameter. If set to true, event is not
     * saved to database, only added to the model.
     * \return true if successful
     */
    virtual bool addEvents(QList<Event> &events, bool toModelOnly = false);

    /*!
     * Modify an event. This will update the event with a matching id in
     * the database.
     * NOTE: group_id changes will always be ignored. local_uid and
     * remote_uid can only be changed for draft events.
     * event.lastModified() is automatically updated.
     * \param event Event to be modified.
     * \return true if successful
     */
    virtual bool modifyEvent(Event &event);

    /*!
     * Modify many events at once. See modifyEvent().
     * \param events Events to be modified.
     * \return true if successful
     */
    virtual bool modifyEvents(QList<Event> &events);

    /*!
     * Delete an event from the model and the database.
     * \param id id of the event to be deleted.
     * \return true if successful
     */
    Q_INVOKABLE virtual bool deleteEvent(int id);

    /*!
     * Delete an event from the model and the database.
     * \param Event Valid event to be deleted.
     * \return true if successful
     */
    virtual bool deleteEvent(Event &event);

    /*!
     * Modify events belonging to the group and updates group model properties.
     * queries
     * \param events Events to be modified.
     * \param group Events' group
     */
    bool modifyEventsInGroup(QList<Event> &events, Group group);

    /*!
     * Updates groupId of the event. Modifies database, emits added/deleted signals for the event.
     * Deletes group if it was the last event in a group.
     *
     * \param event Event to be changed.
     * \param groupId new group Id.
     * \return true if successful
     */
    bool moveEvent(Event &event, int groupId);

    /*!
     * In StreamedAsyncQuery mode, returns true if the tracker query has
     * more data available.
     * \param parent parent index. Implementation depends on submodel.
     * \return true if more chunks can be fetched.
     */
    virtual bool canFetchMore(const QModelIndex &parent) const;

    /*!
     * In StreamedAsyncQuery mode, fetches the next chunk of events if
     * available.
     * \param parent parent index. Implementation depends on submodel.
     */
    virtual void fetchMore(const QModelIndex &parent);

    virtual bool isTree() const;
    virtual QueryMode queryMode() const;
    virtual uint chunkSize() const;
    uint firstChunkSize() const;
    virtual int limit() const;
    virtual int offset() const;
    virtual bool isReady() const;

    /*** reimp from QAbstractItemModel ***/
    virtual QModelIndex parent(const QModelIndex &index) const;

    virtual bool hasChildren(const QModelIndex &parent = QModelIndex()) const;

    virtual QModelIndex index(int row, int column,
                              const QModelIndex &parent = QModelIndex()) const;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    virtual QVariant headerData(int section, Qt::Orientation orientation,
                                int role) const;

    virtual QHash<int, QByteArray> roleNames() const;

    /*!
     * Provide background thread for running database queries and blocking operations.
     * It allows to avoid blocking when the model used in the main GUI thread.
     * This function will cancel any outgoing requests. If thread is NULL,
     * model's thread is used for quereis.
     *
     * The thread should be started before making any queries and it should not
     * be terminated before deleting the model. Client is responsible for thread
     * termination and deleting.
     *
     * \param thread running thread
    **/
    void setBackgroundThread(QThread *thread);
    QThread* backgroundThread();

    /*!
     * Return an instance of DatabaseIO that can be used for low-level queries.
     * \return a DatabaseIO
     */
    DatabaseIO& databaseIO();

Q_SIGNALS:
    /*!
     * Emitted when a query is finished and the model has been filled.
     *
     * \param successful or false in case of an error
     *
     */
    void modelReady(bool successful);

    /*!
     * Emitted when event operation finishes:
     * addEvent, modifyEvent(s) will emit this signal once the modifications committed
     * to the underlying storage.
     *
     * \param events committed events
     * \param successful or false in case of an error
     */
    void eventsCommitted(const QList<CommHistory::Event> &events, bool successful);

protected:
    EventModelPrivate * const d_ptr;
    EventModel(EventModelPrivate &dd, QObject *parent = 0);

private:
    Q_DECLARE_PRIVATE(EventModel);
};

}

#endif
