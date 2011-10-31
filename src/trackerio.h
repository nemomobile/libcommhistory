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

#ifndef COMMHISTORY_TRACKERIO_H
#define COMMHISTORY_TRACKERIO_H

#include <QObject>
#include <QUrl>

#include "event.h"
#include "libcommhistoryexport.h"

class QSparqlResult;

namespace CommHistory {

class TrackerIOPrivate;
class Group;
class UpdateQuery;
class CommittingTransaction;

/**
 * \class TrackerIO
 *
 * Class for handling events with tracker. You can use this if you are
 * implementing your own model.
 */
class LIBCOMMHISTORY_EXPORT TrackerIO : public QObject
{
    Q_OBJECT

public:
    TrackerIO();
    ~TrackerIO();
    static TrackerIO* instance();

    /*!
     * Returns and increases the next available event id.
     */
    int nextEventId();

    /*!
     * Add a new event into the database. The id field of the event is
     * updated if successfully added.
     *
     * \param event New event.
     * \return true if successful, otherwise false
     */
    bool addEvent(Event &event);

    /*!
     * Add a new group into the database. The id field of the group is
     * updated if successfully added.
     *
     * \param group New group.
     * \return true if successful, otherwise false
     */
    bool addGroup(Group &group);

    /*!
     * Query a single event by id.
     *
     * \param id Database id of the event.
     * \param event Return value for event details.
     * \return true if successful, otherwise false
     */
    bool getEvent(int id, Event &event);

    /*!
     * Query a single event by uri.
     *
     * \param Uri of the message to be fetched
     * \param event Return value for event details.
     * \return true if successful, otherwise false
     */
    bool getEventByUri(const QUrl &uri, Event &event);

    /*!
     * Query a single event by message token.
     *
     * \param token Message token
     * \param event Return value for event details.
     * \return true if successful, otherwise false
     */
    bool getEventByMessageToken(const QString &token, Event &event);

    /*!
     * Query a single event by message token and group ID.
     *
     * \param token Message token
     * \param groupId Group ID
     * \param event Return value for event details.
     * \return true if successful, otherwise false
     */
    bool getEventByMessageToken(const QString &token, int groupId, Event &event);

    /*!
     * Query a single event by mms id.
     *
     * \param mmsId mms id
     * \param groupId Group ID
     * \param event Return value for event details.
     * \return true if successful, otherwise false
     */
    bool getEventByMmsId(const QString &mmsId, int groupId, Event &event);

    /*!
     * Modifye an event.
     *
     * \param event Existing event.
     * \return true if successful, otherwise false
     */
    bool modifyEvent(Event &event);

    /*!
     * Move an event to a new group
     *
     * \param event Existing event
     * \param groupId new group id
     *
     * \return true if successful, otherwise false
     */
    bool moveEvent(Event &event, int groupId);

    /*!
     * Delete an event
     *
     * \param event Existing event to delete
     * \param backgroundThread optional thread (to delete mms attachments)
     *
     * \return true if successful, otherwise false
     */
    bool deleteEvent(Event &event, QThread *backgroundThread = 0);

    /*!
     * Query a single group by id.
     *
     * \param id Database id of the group.
     * \param group Return value for group details.
     * \return true if successful, otherwise false
     */
    bool getGroup(int id, Group &group);

    /*!
     * Modifye a group.
     *
     * \param event Existing group.
     * \return true if successful, otherwise false
     */
    bool modifyGroup(Group &group);

    /*!
     * Delete a group
     *
     * \param groupId Existing group id
     * \param deleteMessages flag to delete group's messages
     * \param backgroundThread optional thread (to delete mms attachments)
     *
     * \return true if successful, otherwise false
     */
    bool deleteGroup(int groupId, bool deleteMessages = true, QThread *backgroundThread = 0);

    /*!
     * Delete groups
     *
     * \param groupIds Existing group ids
     * \param deleteMessages flag to delete group's messages
     * \param backgroundThread optional thread (to delete mms attachments)
     *
     * \return true if successful, otherwise false
     */
    bool deleteGroups(QList<int> groupIds, bool deleteMessages = true, QThread *backgroundThread = 0);

    /*!
     * Query the number of events in a group
     *
     * \param groupId Existing group id
     * \param totalEvents result
     *
     * \return true if successful, otherwise false
     */
    bool totalEventsInGroup(int groupId, int &totalEvents);

    /*!
     * Mark all messages in a group as read
     *
     * \param groupId Existing group id
     *
     * \return true if successful, otherwise false
     */
    bool markAsReadGroup(int groupId);

    /*!
     * Mark all calls in the same call group as read
     *
     * \param event Call event
     *
     * \return true if successful, otherwise false
     */
    bool markAsReadCallGroup(Event &event);

    /*!
     * Mark messages as read
     *
     * \param eventIds list of events to mark
     *
     * \return true if successful, otherwise false
     */
    bool markAsRead(const QList<int> &eventIds);

    /*!
     * Mark all messages of a certain type as read
     *
     * \param eventType
     *
     * \return true if successful. Sets lastError() on failure.
     */
    bool markAsReadAll(Event::EventType eventType);

    /*!
     * Delete events of a certain type
     *
     * \param eventType
     *
     * \return true if successful, otherwise false
     */
    bool deleteAllEvents(Event::EventType eventType);

    /*!
     * Initate a new tracker transaction.
     *
     * \param syncOnCommit set to perform tracker sync after commit
     */
    void transaction(bool syncOnCommit = false);

    /*!
     * Commits the current transaction.
     *
     * \param isBlocking if true, the call blocks until changes are saved
     *                   if false, the call is asynchronous and returns immediately
     * \return transaction object to track commit progress for non-blocking call
     */
    CommittingTransaction* commit(bool isBlocking=false);

    /*!
     * Cancels the current transaction.
     */
    void rollback();

    /*!
     * Do NOT call this unless you know what you are doing.
     */
    void recreateIds();

    /*!
     * Get the ongoing transaction.
     */
    CommittingTransaction *currentTransaction() const;

private:
    friend class TrackerIOPrivate;
    friend class QueryRunner;
    TrackerIOPrivate * const d;
};

} // namespace

#endif
