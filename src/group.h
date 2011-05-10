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

#ifndef COMMHISTORY_GROUP_H
#define COMMHISTORY_GROUP_H

#include <QSharedDataPointer>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QStringList>
#include <QDateTime>
#include <QDebug>

#include "event.h"
#include "libcommhistoryexport.h"

class QDBusArgument;

namespace CommHistory {

class GroupPrivate;

/*!
 * \class Group
 *
 * Conversation (or, group chat) data.
 */
class LIBCOMMHISTORY_EXPORT Group
{
    Q_ENUMS(CommHistory::Group::ChatType);

public:
    enum ChatType {
        ChatTypeP2P = 0,
        ChatTypeUnnamed, // Tp::HandleTypeNone MUCs such as Skype or MSN
        ChatTypeRoom     // Jabber MUC or other group chat
    };

    enum Property {
        Id = 0,        // always valid
        LocalUid,
        RemoteUids,
        Type,
        ChatName,
        EndTime,
        TotalMessages,
        UnreadMessages,
        SentMessages,
        LastEventId,
        ContactId, // TODO: remove
        ContactName, // TODO: remove
        LastMessageText,
        LastVCardFileName,
        LastVCardLabel,
        LastEventType,
        LastEventStatus,
        LastModified,
        StartTime,
        Contacts,
        NumProperties
    };

    typedef QSet<Group::Property> PropertySet;

public:
    Group();
    Group(const Group &other);
    Group &operator=(const Group &other);
    bool operator==(const Group &other) const;
    ~Group();

    static int urlToId(const QString &url);
    static QUrl idToUrl(int id);

    /*!
     * Returns a property set with all properties.
     */
    static Group::PropertySet allProperties();

    /*!
     * Get set of valid properties (i.e. properties that have been
     * assigned a value since the group was created) for this group.
     *
     * \return Set of valid properties.
     */
    Group::PropertySet validProperties() const;

    /*!
     * Set valid properties. API users should not normally need this, as
     * properties are automatically marked valid when modified.
     *
     * \param properties New set of properties.
     */
    void setValidProperties(const Group::PropertySet &properties);

    /*!
     * Get set of modified properties.
     *
     * \return Set of modified properties.
     */
    Group::PropertySet modifiedProperties() const;

    /*!
     * Reset modified properties. API users should not normally need this.
     *
     * \param properties New set of properties.
     */
    void resetModifiedProperties();

    bool isValid() const;

    /*!
     * Database id.
     */
    int id() const;

    /*!
     * Tracker url of the group.
     */
    QUrl url() const;

    /*!
     * Local account uid.
     */
    QString localUid() const;

    /*!
     * Remote contacts participating in this conversation.
     * NOTE: QString("") is interpreted as a hidden/private phone number.
     */
    QStringList remoteUids() const;

    /*!
     * Chat type (roughly corresponds to Telepathy handle type).
     * Default for new groups is Group::ChatTypeP2P.
     */
    ChatType chatType() const;

    /*!
     * Name for the chat room / group chat (optional).
     */
    QString chatName() const;

    /*!
     * Start time of the last message.
     * For incoming messages this is the time message was sent. For
     * outgoing messages this is the time message was delivered (or sent
     * if delivery reporting is not enabled).
     */
    QDateTime startTime() const;

    /*!
     * End time of the last message.
     * For incoming messages this is the time message was received.
     * For outgoing messages this is the time message was sent.
     */
    QDateTime endTime() const;

    /*!
     * Number of messages in this conversation.
     */
    int totalMessages() const;

    /*!
     * Number of unread messages in this conversation.
     */
    int unreadMessages() const;

    /*!
     * Number of sent messages in this conversation.
     */
    int sentMessages() const;

    /*!
     * Database id of the last message. -1 if the group has no messages.
     */
    int lastEventId() const;

    /*!
     * DEPRECATED - use contacts(). Returns the id of the first contact.
     * Id of the remote contact in this conversation.
     * This property is not stored in the database. It is filled in by
     * the model at runtime, if possible.
     */
    int contactId() const;

    /*!
     * DEPRECATED - use contacts(). Returns the name of the first contact.
     * Name of the remote contact in this conversation.
     * This property is not stored in the database. It is filled in by
     * the model at runtime, if possible.
     */
    QString contactName() const;

    /*!
     * Ids and names for the contacts in this conversation.
     * This property is not stored in the database. It is filled in by
     * the model at runtime, if possible.
     */
    QList<Event::Contact> contacts() const;

    /*!
     * Text of the last message.
     * This property is not stored in the database. It is filled in by
     * the model at runtime, if possible.
     */
    QString lastMessageText() const;

    /*!
     * Filename of the vcard in the last event, if any.
     * Notice: this is not the last vcard in a group, but the vcard in the last message, which
     * might of course be empty.
     */
    QString lastVCardFileName() const;

    /*!
     * Label of the vcard in the last event, if any.
     * Notice: this is not the last vcard in a group, but the vcard in the last message, which
     * might of course be empty.
     */
    QString lastVCardLabel() const;

    /*!
     * Type of the last sent / received message. @see Event::EventType
     */
    Event::EventType lastEventType() const;

    /*!
     * Status of last message, for received messages status is always equal
     * to Event::UnknownStatus
     */
    Event::EventStatus lastEventStatus() const;

    QDateTime lastModified() const;

    void setId(int id);
    void setLocalUid(const QString &uid);
    void setRemoteUids(const QStringList &uids);
    void setChatType(Group::ChatType chatType);
    void setChatName(const QString &name);
    void setStartTime(const QDateTime &startTime);
    void setEndTime(const QDateTime &endTime);
    void setTotalMessages(int total);
    void setUnreadMessages(int unread);
    void setSentMessages(int sent);
    void setLastEventId(int id);
    /* DEPRECATED - use setContacts() */
    void setContactId(int id);
    /* DEPRECATED - use setContacts() */
    void setContactName(const QString &name);
    void setContacts(const QList<Event::Contact> &contacts);
    void setLastMessageText(const QString &text);
    void setLastVCardFileName(const QString &filename);
    void setLastVCardLabel(const QString &label);
    void setLastEventType(Event::EventType eventType);
    void setLastEventStatus(Event::EventStatus eventStatus);
    void setLastModified(const QDateTime &modified);

    QString toString() const;

private:
    QSharedDataPointer<GroupPrivate> d;
};

}

QDBusArgument &operator<<(QDBusArgument &argument, const CommHistory::Group &group);
const QDBusArgument &operator>>(const QDBusArgument &argument, CommHistory::Group &group);

LIBCOMMHISTORY_EXPORT QDataStream &operator<<(QDataStream &stream, const CommHistory::Group &group);
LIBCOMMHISTORY_EXPORT QDataStream &operator>>(QDataStream &stream, CommHistory::Group &group);

Q_DECLARE_METATYPE(CommHistory::Group);
Q_DECLARE_METATYPE(QList<CommHistory::Group>);
Q_DECLARE_METATYPE(QList<int>)

#endif
