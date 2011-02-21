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

#ifndef COMMHISTORY_EVENT_H
#define COMMHISTORY_EVENT_H

#include <QSharedDataPointer>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QVariant>
#include <QSet>

#include "messagepart.h"
#include "libcommhistoryexport.h"

class QDBusArgument;

namespace CommHistory {

    enum ParentIds
    {
        ALL = -1,
        INBOX = 0x1002,
        OUTBOX = 0x1003,
        DRAFT = 0x1004,
        SENT = 0x1005,
        MYFOLDER = 0x1008
   };

class EventPrivate;

/*!
 * \class Event
 *
 * Instant message, SMS or call data.
 */
class LIBCOMMHISTORY_EXPORT Event
{
public:
    // TODO : create specified event classes like ImEvent, SmsEvent, CallEvent
    //        the current Event would be an abstract base class of these derived classes
    //        all needless properties would be hidden + accessible ones would be type specific:
    //          - CallEvent      : startTime, endTime, *does not have* text, *no need for* status, isMissed
    //          - SmsEvent       : sentTime, deliveredTime, freeText, isDraft, *all listed* status
    //          - ImEvent        : sentTime, freeText, htmlText, *limited* status (unknown/sent/failed)
    //          - VoicemailEvent : TODO this type isn't supported in tracker yet. Only commhistoryd uses it internally at the moment
    // TODO : ^^^
    enum EventType
    {
        UnknownType = 0,
        IMEvent,
        SMSEvent,
        CallEvent,
        VoicemailEvent,
        /*!
         * Type of the events which represent the status message of the remote party in the
         * conversation. These events must be added to model with toModelOnly=true option. Note that
         * all items which are added to data model but not saved to database, are lost on the next
         * fetching.
         */
        StatusMessageEvent,
        MMSEvent,
        /*!
         * Event that could be added to ClassZeroSMSModel, not persistent.
         * Similar to StatusMessageEvent
         */
        ClassZeroSMSEvent
    };

    enum EventDirection
    {
        UnknownDirection = 0,
        Inbound,
        Outbound
    };

    enum EventStatus {
        UnknownStatus = 0,
        // TODO: Sending is not a valid state for nmo:deliveryStatus, so
        // a missing property is interpreted as SendingStatus.
        SendingStatus,
        SentStatus,
        DeliveredStatus,
        // deprecate FailedStatus
        FailedStatus,
        TemporarilyFailedStatus = FailedStatus,
        PermanentlyFailedStatus
    };

    enum EventReadStatus {
        UnknownReadStatus = 0,
        ReadStatusRead    = 1,
        ReadStatusDeleted = 2
    };

    enum Property {
        Id = 0,        // always valid
        Type,          // always valid
        StartTime,
        EndTime,
        Direction,
        IsDraft,
        IsRead,
        IsMissedCall,
        IsEmergencyCall,
        Status,
        BytesReceived,
        LocalUid,
        RemoteUid,
        ContactId,
        ContactName,
        ParentId,
        Subject,
        FreeText,
        GroupId,
        MessageToken,
        LastModified,
        EventCount,
        FromVCardFileName,
        FromVCardLabel,
        Encoding,
        CharacterSet,
        Language,
        IsDeleted,
        ReportDelivery,
        ValidityPeriod,
        ContentLocation,
        MessageParts,
        Cc,
        Bcc,
        ReadStatus,
        ReportRead,
        ReportReadRequested,
        MmsId,
        To,
        //
        NumProperties
    };

    typedef QSet<Event::Property> PropertySet;

public:
    Event();
    Event(const Event &other);
    ~Event();

    Event &operator=(const Event &other);
    bool operator==(const Event &other) const;
    bool operator!=(const Event &other) const;
    bool isValid() const;

    /*!
     * Extracts event id from tracker url.
     */
    static int urlToId(const QString &url);

    /*!
     * Translate event id to tracker url.
     */
    static QUrl idToUrl(int id);

    /*!
     * Returns a property set with all properties.
     */
    static Event::PropertySet allProperties();

    /*!
     * Get set of valid properties (i.e. properties that have been
     * assigned a value since the event was created) for this event.
     *
     * \return Set of valid properties.
     */
    Event::PropertySet validProperties() const;

    /*!
     * Set valid properties. API users should not normally need this, as
     * properties are automatically marked valid when modified.
     *
     * \param properties New set of properties.
     */
    void setValidProperties(const Event::PropertySet &properties);

    /*!
     * Get set of modified properties. It's usually reset after addEvent
     * or modifyEvent.
     *
     * \return Set of modified properties.
     */
    Event::PropertySet modifiedProperties() const;

    /*!
     * Reset modified properties. API users should not normally need this.
     *
     * \param properties New set of properties.
     */
    void resetModifiedProperties();

    //\\//\\// G E T - A C C E S S O R S //\\//\\//
    int id() const;

    QUrl url() const;

    Event::EventType type() const;

    /*!
     * Gets the start time of a call, sent/sending/failed time of SMS/IM
     *
     * \return Timestamp.
     */
    QDateTime startTime() const;

    /*!
     * Gets the end time of a call, sent/sending/failed time of  SMS/IM (if delivery reports are enabled: delivered time of SMS)
     *
     * \return Timestamp.
     */
    QDateTime endTime() const;

    Event::EventDirection direction() const;

    bool isDraft() const;

    bool isRead() const;

    bool isMissedCall() const;

    bool isEmergencyCall() const;

    Event::EventStatus status() const;

    int bytesReceived() const;

    QString localUid() const;  /* telepathy account */

    QString remoteUid() const;

    int contactId() const;

    QString contactName() const;

    int parentId() const; // SMS parent folder id

    QString subject() const;

    QString freeText() const;

    int groupId() const; /* communication channel */

    QString messageToken() const;
    QString mmsId() const;

    QDateTime lastModified() const;

    /*!
     * \brief Gets how many similar call events are represented by the current event.
     *
     * This property gets how many call events are grouped under the current one.
     * The value is set and used by the CallHistoryCallModel and call-history
     * application.
     *
     * NOTE: This property has meaningful value only in case of grouped call
     * events. In all other case, the value should be ignored.
     *
     * \return The number of similar events in the same call event group.
     */
    int eventCount() const;

    QString fromVCardFileName() const;

    QString fromVCardLabel() const;

    QString encoding() const;

    QString characterSet() const;

    QString language() const;

    bool isDeleted() const;

    bool reportDelivery() const;

    bool reportRead() const;

    bool reportReadRequested() const;

    Event::EventReadStatus readStatus() const;

    int validityPeriod() const;

    // MMS content location URL, for MMS notifications
    QString contentLocation() const;

    QList<MessagePart> messageParts() const;

    // NOTE: Cc and Bcc will not be initialized in a getEvents() model
    // query. You have to fetch the full event data with getEvent() to
    // access these properties.
    QStringList toList() const;
    QStringList ccList() const;
    QStringList bccList() const;

    //\\//\\// S E T - A C C E S S O R S //\\//\\//
    void setId(int id);

    void setType(Event::EventType type);

    void setStartTime(const QDateTime &startTime);

    void setEndTime(const QDateTime &endTime);

    void setDirection(Event::EventDirection direction);

    void setIsDraft( bool isDraft );

    void setIsRead(bool isRead);

    void setIsMissedCall( bool isMissed );

    void setIsEmergencyCall( bool isEmergency );

    void setStatus(Event::EventStatus status);

    void setBytesReceived(int bytes);

    void setLocalUid(const QString &uid);

    void setRemoteUid(const QString &uid);

    void setContactId(int id);

    void setContactName(const QString &name);

    void setParentId(int id);

    void setSubject(const QString &subject);

    void setFreeText(const QString &text);

    void setGroupId(int id);

    void setMessageToken(const QString &token);

    void setMmsId(const QString &id);

    void setLastModified(const QDateTime &modified);

    /*!
     * \brief Sets the value of how many similar call events are represented by the current event.
     *
     * NOTE: This property has meaningful value only in case of grouped call
     * events. In all other case, the value should be ignored.
     *
     * \return The number of similar events in the same call event group.
     */
    void setEventCount( int count );

    void setFromVCard( const QString &fileName, const QString &label = QString() ); //fromvcard

    void setEncoding(const QString& enc);

    void setCharacterSet(const QString &charset);

    void setLanguage(const QString &language);

    void setDeleted(bool isDel);

    void setReportDelivery(bool reportDeliveryRequested);

    void setReportRead(bool reportRequested);

    void setReportReadRequested(bool reportShouldRequested);

    void setReadStatus(Event::EventReadStatus eventReadStatus);

    void setValidityPeriod(int validity);

    void setContentLocation(const QString &location);

    void setMessageParts(const QList<MessagePart> &parts);

    void addMessagePart(const MessagePart &part);

    void setToList(const QStringList &ccList);

    void setCcList(const QStringList &ccList);

    void setBccList(const QStringList &bccList);

    QString toString() const;

    bool resetModifiedProperty(Event::Property property);

private:
    QSharedDataPointer<EventPrivate> d;
};

}

LIBCOMMHISTORY_EXPORT QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::Event &event);
LIBCOMMHISTORY_EXPORT const QDBusArgument &operator>>(const QDBusArgument &argument, CommHistory::Event &event);

Q_DECLARE_METATYPE(CommHistory::Event);
Q_DECLARE_METATYPE(QList<CommHistory::Event>);

#endif
