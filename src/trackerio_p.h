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

#ifndef COMMHISTORY_TRACKERIO_P_H
#define COMMHISTORY_TRACKERIO_P_H

#include <QObject>
#include <QUrl>
#include <QHash>
#include <QThreadStorage>
#include <QSparqlQuery>
#include <QQueue>
#include <QThread>
#include <QThreadStorage>

#include "idsource.h"
#include "event.h"
#include "commonutils.h"

class MmsContentDeleter;
class QSparqlConnection;
class QSparqlResult;

namespace CommHistory {

class Group;
class UpdateQuery;
class TrackerIO;
class CommittingTransaction;
class EventsQuery;

/**
 * \class TrackerIOPrivate
 *
 * Private data and methods for TrackerIO
 */
class TrackerIOPrivate : public QObject
{
    Q_OBJECT
    TrackerIO *q;

public:
    TrackerIOPrivate(TrackerIO *parent);
    ~TrackerIOPrivate();

    static QUrl uriForIMAddress(const QString &account, const QString &remoteUid);

    /*!
     * Returns and increases the next available group id.
     */
    int nextGroupId();

    /*!
     * Builds a tracker callgroup URI for the event.
     */
    static QString makeCallGroupURI(const CommHistory::Event &event);

    /*!
     * Adds required message part properties to the query.
     */
    static QString prepareMessagePartQuery(const QString &messageUri);

    /*!
     * Adds required message part properties to the query.
     */
    static QString prepareGroupQuery(const QString &localUid = QString(),
                                     const QString &remoteUid = QString(),
                                     int groupId = -1);

    /*!
     * Create query for calls grouped by contacts.
     */
    static QString prepareGroupedCallQuery();

    /*!
     * Return IMContact node as blank anonymous SPARQL string
     * that corresponds to account/target (or
     * account if imID is empty), creating if necessary. Uses internal
     * cache during a transaction
     */
    QString findLocalContact(UpdateQuery &query,
                             const QString &accountPath);

    /*!
     * Modify the query to take care of adding a suitable anon blank contact for
     * the specified property and make sure that the IMAddress or phone number exists.
     */
    void addIMContact(UpdateQuery &query,
                      const QUrl &subject,
                      const char *predicate,
                      const QString &accountPath,
                      const QString &imID);

    void addPhoneContact(UpdateQuery &query,
                         const QUrl &subject,
                         const char *predicate,
                         const QString &phoneNumber,
                         PhoneNumberNormalizeFlags normalizeFlags);

    void addRemoteContact(UpdateQuery &query,
                          const QUrl &subject,
                          const char *predicate,
                          const QString &localUid,
                          const QString &remoteUid,
                          PhoneNumberNormalizeFlags normalizeFlags
                          = NormalizeFlagRemovePunctuation);

    /*!
     * Helper for inserting and modifying common parts of nmo:Messages.
     *
     * \param query query to add RDF insertions or deletions
     * \param event to use
     * \param modifyMode if true, event.modifiedProperties are used to save
     *                   only changed properties, otherwise event.validProperties
     *                   is used to write all properties.
     */
    void writeCommonProperties(UpdateQuery &query, Event &event, bool modifyMode);
    void writeSMSProperties(UpdateQuery &query, Event &event, bool modifyMode);
    void writeMMSProperties(UpdateQuery &query, Event &event, bool modifyMode);
    void writeCallProperties(UpdateQuery &query, Event &event, bool modifyMode);

    void addMessageParts(UpdateQuery &query, Event &event);
    /*!
     * Sets the group for the event into tracker.
     *
     * \param query query to add insertions
     * \param event to use
     * \param channelId the group id
     * \param modify true -> if channelId is going to be just updated for an event here,
     *               false -> if channelId is set for the event for the first time
     */
    void setChannel(UpdateQuery &query, Event &event, int channelId, bool modify = false);

    /* Used by addEvent(). */
    void addIMEvent(UpdateQuery &query, Event &event);
    void addSMSEvent(UpdateQuery &query, Event &event); // also handles MMS
    void addCallEvent(UpdateQuery &query, Event &event);


    // Helper for getEvent*().
    bool querySingleEvent(EventsQuery &query, Event &event);

    static void calculateParentId(Event& event);
    static void setFolderLastModifiedTime(UpdateQuery &query,
                                          int parentId,
                                          const QDateTime& lastModTime);

    QSparqlResult* getMmsListForDeletingByGroup(int groupId);
    bool deleteMmsContentByGroup(int group);
    MmsContentDeleter& getMmsDeleter(QThread *backgroundThread);
    bool isLastMmsEvent(const QString& messageToken);

    void checkAndDeletePendingMmsContent(QThread* backgroundThread);

    QSparqlConnection& connection();
    bool checkPendingResult(QSparqlResult *result, bool destroyOnFinished = true);
    bool handleQuery(const QSparqlQuery &query,
                     QObject *caller = 0,
                     const char *callback = 0,
                     QVariant argument = QVariant());
    bool runBlockedQuery(QSparqlResult *result);

public Q_SLOTS:
    void runNextTransaction();
    /*!
     * Update nmo:lastMessageDate and nmo:lastSuccessfulMessageDate for
     * channel and delete empty call groups.
     */
    void doUpdateGroupTimestamps(CommittingTransaction *transaction,
                                 QSparqlResult *result,
                                 QVariant arg);
    void updateGroupTimestamps(CommittingTransaction *transaction,
                               QSparqlResult *result,
                               QVariant arg);

    void requestCountMmsEvents();
    void doCleanMmsGarbage(CommittingTransaction *transaction,
                           QSparqlResult *result,
                           QVariant arg);

public:
    QThreadStorage<QSparqlConnection*> m_pConnection;
    CommittingTransaction *m_pTransaction;
    QQueue<CommittingTransaction*> m_pendingTransactions;

    // Temporary contact cache, valid during a transaction
    QHash<QUrl, QString> m_contactCache;
    MmsContentDeleter *m_MmsContentDeleter;
    typedef QHash<QString, int> MessageTokenRefCount;
    MessageTokenRefCount m_messageTokenRefCount;
    bool syncOnCommit;

    IdSource m_IdSource;

    Event::PropertySet commonPropertySet;
    Event::PropertySet smsOnlyPropertySet;

    QThread *m_bgThread;
};

} // namespace

#endif // COMMHISTORY_TRACKERIO_P_H
